/* NAME: Michael Huo */
/* EMAIL: EMAIL */
/* ID: UID */
#include <sys/socket.h>
#include <netdb.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <zlib.h>

#define PORT_TEMP 8000
#define READ 0
#define WRITE 1
#define CLIENT 0
#define SHELL_OUT 1

int write_input(int fd, char c) {
    if(c == 0x04) //EOF
        return 1;
    else if(c == 0x03) //^C
        return 2;
    if(c == 0x0D || c == 0x0A) {
        //write(STDOUT_FILENO, "\x0D\x0A", 2);
        if(fd >= 0) write(fd, "\x0A", 1);
    }
    else {
        //write(STDOUT_FILENO, &c, 1);
        if(fd >= 0) write(fd, &c, 1);
    }
    return 0;
}

void handle_error(char* name, char* details) {
    fprintf(stderr, "%s: %s: %s\n", name, details, strerror(errno));
    exit(1);
}

int main(int argc, char** argv) {
    char *endptr, *shell = 0;
    int port, port_flag = 0, compress = 0;
    int c;
    static struct option long_options[] = {
            {"shell", required_argument, 0, 's'},
            {"port", required_argument, 0, 'p'},
            {"compress", no_argument, 0, 'c'},
            {0, 0, 0,0}
    };

    while ((c = getopt_long(argc, argv, "", long_options, 0)) != -1) {
        switch(c) {
            case 's':
                shell = optarg;
                break;
            case 'p':
                port = strtol(optarg, &endptr, 10);
                port_flag = 1;
                break;
            case 'c':
                compress = 1;
                break;
            default:
                fprintf(stderr, "usage: %s [--shell=<path/to/program>]\n", argv[0]);
                exit(1);
        }
    }

    //*endptr expression is checking if port argument was successfully parsed - it should point to '\0'
    if (!port_flag || *endptr || port < 0 || port > 65535) {
        fprintf(stderr, "%s: must specify valid port (--port=<port>) in range [0, 65535]\n", argv[0]);
        exit(1);
    }
    if (!shell) { //behavior not obvious and not defined by spec, so we'll terminate
        fprintf(stderr, "%s: must specify shell program (--shell=<path/to/program>)\n", argv[0]);
        exit(1);
    }

    z_stream strm_out, strm_in;
    unsigned char zbuf[256];
    int z_ret, z_flush, bytes_processed;
    if (compress) {
        strm_out.zalloc = Z_NULL;
        strm_out.zfree = Z_NULL;
        strm_out.opaque = Z_NULL;

        strm_in.zalloc = Z_NULL;
        strm_in.zfree = Z_NULL;
        strm_in.opaque = Z_NULL;

        if(deflateInit(&strm_out, Z_DEFAULT_COMPRESSION) != Z_OK ||
           inflateInit(&strm_in) != Z_OK)
            handle_error(argv[0], "error initializing zlib (de)compression stream(s)");
    }

    struct sockaddr_in addr = {AF_INET, htons(port), {INADDR_ANY}, {0}};
    //0 initialize sin_zero, which is padding for sockaddr_in - avoids gcc warning

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) handle_error(argv[0], "error opening socket");

    if(bind(sfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1)
        handle_error(argv[0], "error binding address/port");

    listen(sfd, 5);
    int newsfd = accept(sfd, NULL, NULL);

    int shell_in[2]; //pipe from terminal into shell
    int shell_out[2];
    pid_t pid;

    if(pipe(shell_in) == -1 || pipe(shell_out) == -1) {
        handle_error(argv[0], "error opening pipe(s)");
    }

    pid = fork();

    if (pid < 0)
        handle_error(argv[0], "error forking process");
    else if (pid == 0) { //forked process - this will execute shell
        close(shell_in[WRITE]); //don't need to write to its input pipe
        close(shell_out[READ]); //don't need to read from its output

        int stderr_copy = dup(STDERR_FILENO);

        if((stderr_copy = dup(STDERR_FILENO)) == -1 ||
            dup2(shell_in[READ], STDIN_FILENO) == -1 ||
            dup2(shell_out[WRITE], STDOUT_FILENO) == -1 ||
            dup2(shell_out[WRITE], STDERR_FILENO) == -1)
            handle_error(argv[0], "error duping file descriptor(s)");

        if(execl(shell, shell, (char*) NULL) == -1){
            dprintf(stderr_copy, "%s: error executing %s: %s\n", argv[0], shell, strerror(errno));
            exit(1);
        }
    }
    else { //server process
        close(shell_in[READ]); //same as above
        close(shell_out[WRITE]);

        struct pollfd fds[2];

        fds[CLIENT].fd = newsfd;
        fds[CLIENT].events = POLLIN;
        fds[SHELL_OUT].fd = shell_out[READ];
        fds[SHELL_OUT].events = POLLIN;

        signal(SIGPIPE, SIG_IGN); //don't want to exit/crash from writing to closed pipe

        int wstatus;
        int poll_ret, bytes_read, status_flag = 0; //status_flag is EOF indicator
        unsigned char *kb_buffer, *shell_buffer;
        kb_buffer = malloc(16);
        shell_buffer = malloc(256);
        while (1) {
            poll_ret = poll(fds, 2, 0);

            if (poll_ret == -1)
                handle_error(argv[0], "polling error");
            else if (poll_ret > 0) {
                if (fds[CLIENT].revents & POLLIN) {
                    bytes_read = read(fds[CLIENT].fd, kb_buffer, 16);

                    if(bytes_read <= 0) { //EOF or read error
                        close(shell_in[WRITE]);
                        fds[CLIENT].events = 0; //no longer interested in input from client, since EOF received
                    }

                    if (compress) {
                        strm_in.avail_in = bytes_read;
                        strm_in.next_in = kb_buffer;

                        do {
                            strm_in.avail_out = 256;
                            strm_in.next_out = zbuf;
                            z_ret = inflate(&strm_in, Z_SYNC_FLUSH);
                            if(z_ret != Z_OK && z_ret != Z_STREAM_END && z_ret != Z_BUF_ERROR) {
                                fprintf(stderr, "%s: error in zlib inflation with return code %d - check zlib "
                                                "return codes for more details\n", argv[0], z_ret);
                                exit(1);
                            }
                            bytes_processed = 256 - strm_in.avail_out;
                            for (int i = 0; i < bytes_processed; ++i) {
                                //fprintf(stderr,"%d", zbuf[i]);
                                //break;
                                if((status_flag = write_input(shell_in[WRITE], zbuf[i]))){
                                    if(status_flag == 1) {
                                        close(shell_in[WRITE]);
                                        break;
                                    }
                                    else if(status_flag == 2) {
                                        kill(pid, SIGINT);
                                        break;
                                    }
                                }
                            }
                        } while (strm_in.avail_out == 0);
                    }
                    else {
                        for (int i = 0; i < bytes_read; ++i) {
                            if ((status_flag = write_input(shell_in[WRITE], kb_buffer[i]))) {
                                if (status_flag == 1) {
                                    close(shell_in[WRITE]);
                                    break;
                                } else if (status_flag == 2) {
                                    kill(pid, SIGINT);
                                    break;
                                }
                            }
                        }
                    }
                }
                else if (fds[CLIENT].revents & (POLLHUP | POLLERR)) {
                    close(shell_in[WRITE]);
                    fds[CLIENT].events = 0;
                }
                if (fds[SHELL_OUT].revents & POLLIN) {
                    bytes_read = read(shell_out[READ], shell_buffer, 256);

                    if (bytes_read <= 0) { //shell is in EOF state
                        close(shell_in[WRITE]);
                        if ((waitpid(pid, &wstatus, WUNTRACED) == -1))
                            handle_error(argv[0], "waitpid syscall error");

                        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n",
                                WTERMSIG(wstatus), WEXITSTATUS(wstatus));
                        if (compress) {
                            inflateEnd(&strm_in);
                            deflateEnd(&strm_out);
                        }
                        close(sfd);
                        close(newsfd);
                        exit(0);
                    }

                    if (compress) {
                        z_flush = (bytes_read > 0) ? Z_SYNC_FLUSH : Z_FINISH; //FINISH if EOF/no bytes read/error
                        strm_out.avail_in = bytes_read;
                        strm_out.next_in = shell_buffer;

                        do {
                            strm_out.avail_out = 256;
                            strm_out.next_out = zbuf;
                            z_ret = deflate(&strm_out, z_flush);
                            if(z_ret != Z_OK && z_ret != Z_STREAM_END && z_ret != Z_BUF_ERROR) {
                                fprintf(stderr, "%s: error in zlib compression with return code %d - check zlib "
                                                "return codes for more details", argv[0], z_ret);
                                inflateEnd(&strm_in);
                                deflateEnd(&strm_out);
                                exit(1);
                            }

                            bytes_processed = 256 - strm_out.avail_out;
                            if (write(newsfd, zbuf, bytes_processed) != bytes_processed) {
                                handle_error(argv[0], "error writing compressed data");
                            }
                        } while (strm_out.avail_out == 0);
                    }
                    else { //just write bytes directly to client
                        for (int i = 0; i < bytes_read; ++i) {
                            write_input(newsfd, shell_buffer[i]);
//                            if (write_input(newsfd, shell_buffer[i]) == 1) { //EOF
//                                close(shell_out[READ]);
//                                fds[SHELL_OUT].events = 0; //no longer interested in shell output
//                                break;
//                            }
                        }
                    }
                }
                else if(fds[SHELL_OUT].revents & (POLLHUP | POLLERR)) { //still want to finish reading whatever is left
                    if ((waitpid(pid, &wstatus, WUNTRACED) == -1))
                        handle_error(argv[0], "waitpid syscall error");

                    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n",
                            WTERMSIG(wstatus), WEXITSTATUS(wstatus));
                    if (compress) {
                        inflateEnd(&strm_in);
                        deflateEnd(&strm_out);
                    }
                    close(sfd);
                    close(newsfd);
                    exit(0);
                }
            }
        }
    }
}
