/* NAME: Michael Huo */
/* EMAIL: EMAIL */
/* ID: UID */
#include <getopt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <ulimit.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <zlib.h>
#include <fcntl.h>

#define KEYBOARD 0
#define SERV_OUT 1

void handle_error(char* name, char* details, struct termios* term) {
    fprintf(stderr, "%s: %s: %s\n", name, details, strerror(errno));
    if(term) tcsetattr(STDIN_FILENO, TCSANOW, term); //restore original settings if term is not NULL
    exit(1);
}

int write_input(int fd, char c) {
    int flag = 0; //if any write returns -1, flag will be -1 due to |=
    if(c == 0x0D || c == 0x0A) {
        flag |= write(STDOUT_FILENO, "\x0D\x0A", 2);
        if(fd >= 0) flag |= write(fd, "\x0A", 1);
    }
    else {
        flag |= write(STDOUT_FILENO, &c, 1);
        if(fd >= 0) flag |= write(fd, &c, 1);
    }
    if (flag < 0) {
        fprintf(stderr, "write system call error; exiting"); //this is not supposed to happen, though
        exit(1);
    }
    return 0;
}

int main(int argc, char** argv) {
    int port, port_flag = 0, compress = 0;
    char *endptr, *log = NULL;

    int c;
    static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"log", required_argument, 0, 'l'},
            {"compress", no_argument, 0, 'c'},
            {0, 0, 0,0}
    };

    while ((c = getopt_long(argc, argv, "", long_options, 0)) != -1) {
        switch(c) {
            case 'p':
                port = strtol(optarg, &endptr, 10);
                port_flag = 1;
                break;
            case 'l':
                log = optarg;
                break;
            case 'c':
                compress = 1;
                break;
            default:
                fprintf(stderr, "usage: %s [--port=<port>]\n", argv[0]);
                exit(1);
        }
    }
    //*endptr expression is checking if port argument was successfully parsed - it should point to '\0'
    if (!port_flag || *endptr || port < 0 || port > 65535) {
        fprintf(stderr, "%s: must specify valid port (--port=<port>) in range [0, 65535]\n", argv[0]);
        exit(1);
    }

    int log_file = -1;
    if (log) {
        log_file = creat(log, 0666);
        if (log_file == -1) {
            fprintf(stderr, "%s: error opening logfile %s: %s\n", argv[0], log, strerror(errno));
            exit(1);
        }
        if(ulimit(UL_SETFSIZE, 10000) == -1)
            handle_error(argv[0], "error setting ulimit for log file", NULL);
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
            handle_error(argv[0], "error initializing zlib (de)compression stream(s)", NULL);
    }

    struct addrinfo* res = NULL;
    struct addrinfo hints = {0}; //0 initialize - we won't be using all the fields

    hints.ai_family = AF_INET; //ipv4

    int errcode;
    if((errcode = getaddrinfo("localhost", NULL, &hints, &res))) {//gethostname(3) obsolete according to man pages
        fprintf(stderr, "%s: getaddrinfo error: %s", argv[0], gai_strerror(errcode));
        exit(1);
    }

    ((struct sockaddr_in*)res->ai_addr)->sin_port = htons(port);
    int sfd = socket(AF_INET, SOCK_STREAM, 0);

    if(connect(sfd, res->ai_addr, sizeof(struct sockaddr_in)) == -1)
        handle_error(argv[0], "error connecting to server", NULL);

    struct termios term_orig, term_copy;

    if(tcgetattr(STDIN_FILENO, &term_orig) == -1)
        handle_error(argv[0], "error getting terminal attributes", NULL);

    term_copy = term_orig; //store attributes for later

    term_copy.c_iflag = ISTRIP;
    term_copy.c_oflag = 0;
    term_copy.c_lflag = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &term_copy);

    struct pollfd fds[2];

    fds[KEYBOARD].fd = STDIN_FILENO;
    fds[KEYBOARD].events = POLLIN;
    fds[SERV_OUT].fd = sfd;
    fds[SERV_OUT].events = POLLIN;

    int poll_ret, bytes_read, fd1; //fd1 will determine behavior for write_input later
    unsigned char *kb_buffer, *shell_buffer;
    kb_buffer = malloc(17); //one extra byte to hold zero byte, if necessary
    shell_buffer = malloc(257);
    while (1) {
        poll_ret = poll(fds, 2, 0);

        if (poll_ret == -1)
            handle_error(argv[0], "polling error", &term_orig);
        else if (poll_ret > 0) {
            if (fds[KEYBOARD].revents & POLLIN) {
                bytes_read = read(STDIN_FILENO, kb_buffer, 16);
                fd1 = compress ? -1 : sfd;

                if (compress) {
                    z_flush = (bytes_read > 0) ? Z_SYNC_FLUSH : Z_FINISH; //FINISH if EOF/no bytes read/error
                    strm_out.avail_in = bytes_read;
                    strm_out.next_in = kb_buffer;

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
                        if (write(sfd, zbuf, bytes_processed) != bytes_processed) {
                            deflateEnd(&strm_out);
                            handle_error(argv[0], "error writing compressed data", &term_orig);
                        }

                        if(bytes_processed > 0 && log) {
                            //assume that if log was specified and we're still running, log_file is valid to write to
                            dprintf(log_file, "SENT %d BYTES: ", bytes_processed);
                            fsync(log_file); //flush buffers so unbuffered write doesn't result in wrong ordering
                            if (write(log_file, zbuf, bytes_processed) != bytes_processed
                            || write(log_file, "\n", 1) != 1) {
                                deflateEnd(&strm_out);
                                handle_error(argv[0], "error writing compressed data to log", &term_orig);
                            }
                        }
                    } while (strm_out.avail_out == 0);
                }
                else if(bytes_read > 0 && log) { //compress not specified
                    kb_buffer[bytes_read] = '\0'; //so fprintf doesn't run into trouble
                    dprintf(log_file, "SENT %d BYTES: %s\n", bytes_read, kb_buffer);
                }

                for (int i = 0; i < bytes_read; ++i) { //do this to echo to terminal regardless
                    write_input(fd1, kb_buffer[i]);
                }
            }
            if (fds[SERV_OUT].revents & POLLIN) {
                //note: POLLIN is set as long as read() won't block
                //so if socket is in EOF condition, POLLIN will still be set
                bytes_read = read(fds[SERV_OUT].fd, shell_buffer, 256);

                if (bytes_read <= 0) {
                    if (compress) {
                        inflateEnd(&strm_in);
                        deflateEnd(&strm_out);
                    }
                    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
                    exit(0);
                }

                if (log) {
                    dprintf(log_file, "RECEIVED %d BYTES: ", bytes_read);
                    fsync(log_file); //flush buffers so unbuffered write doesn't result in wrong ordering
                    if (write(log_file, shell_buffer, bytes_read) != bytes_read
                        || write(log_file, "\n", 1) != 1) {
                        handle_error(argv[0], "error writing compressed data to log", &term_orig);
                    }
                }

                if (compress) {
                    strm_in.avail_in = bytes_read;
                    strm_in.next_in = shell_buffer;

                    do {
                        strm_in.avail_out = 256;
                        strm_in.next_out = zbuf;
                        z_ret = inflate(&strm_in, Z_SYNC_FLUSH);
                        if(z_ret != Z_OK && z_ret != Z_STREAM_END && z_ret != Z_BUF_ERROR) {
                            fprintf(stderr, "%s: error in zlib inflation with return code %d - check"
                                            " zlib return codes for more details\n", argv[0], z_ret);
                            inflateEnd(&strm_in);
                            deflateEnd(&strm_out);
                            tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
                            exit(1);
                        }
                        bytes_processed = 256 - strm_in.avail_out;
                        for (int i = 0; i < bytes_processed; ++i) {
                            write_input(-1, zbuf[i]); //echo to stdout, with newline translation
                        }
                    } while (strm_in.avail_out == 0);
                }
                else { //no compression
                    for (int i = 0; i < bytes_read; ++i) {
                        write_input(-1, shell_buffer[i]); //echo to stdout
                    }
                }
            }
            else if(fds[SERV_OUT].revents & (POLLHUP | POLLERR)) { //still want to finish reading whatever is left
                if (compress) {
                    inflateEnd(&strm_in);
                    deflateEnd(&strm_out);
                }
                tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
                exit(0);
            }
        }
    }
}
