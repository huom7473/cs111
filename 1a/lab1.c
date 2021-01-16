/* NAME: Michael Huo */
/* EMAIL: EMAIL */
/* ID: UID */
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define READ 0
#define WRITE 1
#define KEYBOARD 0
#define SHELL_OUT 1

//echos to stdout as well as writes to (non-negative) fd (used to feed data into shell)
//handles newline translation for stdout, return condition - returns 1 if ^D
int write_input(int fd, char c) {
    if(c == 0x04) //EOF
        return 1;
    else if(c == 0x03) //^C
        return 2;
    else if(c == 0x0D || c == 0x0A) {
        write(STDOUT_FILENO, "\x0D\x0A", 2);
        if(fd >= 0) write(fd, "\x0A", 1);
    }
    else {
        write(STDOUT_FILENO, &c, 1);
        if(fd >= 0) write(fd, &c, 1);
    }
    return 0;
}

int main(int argc, char** argv) {
    char* shell = 0;
    int c;
    static struct option long_options[] = {
            {"shell", required_argument, 0, 's'},
            {0, 0, 0,0}
    };

    while ((c = getopt_long(argc, argv, "", long_options, 0)) != -1) {
        switch(c) {
            case 's':
                shell = optarg;
                break;
            default:
                fprintf(stderr, "usage: %s [--shell=<path/to/program>]\n", argv[0]);
                exit(1);
        }
    }
    struct termios term_orig, term_copy;

    if(tcgetattr(STDIN_FILENO, &term_orig) == -1) {
        fprintf(stderr, "%s: error getting terminal attributes: %s\n", argv[0], strerror(errno));
        exit(1);
    }
    term_copy = term_orig; //store attributes for later

    term_copy.c_iflag = ISTRIP;
    term_copy.c_oflag = 0;
    term_copy.c_lflag = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &term_copy);

    if(shell) {
        int shell_in[2]; //pipe from terminal into shell
        int shell_out[2];
        pid_t pid;

        if(pipe(shell_in) == -1 || pipe(shell_out) == -1) {
            fprintf(stderr, "%s: error opening pipe(s): %s\n", argv[0], strerror(errno));
            tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
            exit(1);
        }

        pid = fork();

        if (pid < 0) {
            fprintf(stderr, "%s: error forking process: %s\n", argv[0], strerror(errno));
            tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
            exit(1);
        }
        else if (pid == 0) { //forked process - this will execute shell
            close(shell_in[WRITE]); //don't need to write to its input pipe
            close(shell_out[READ]); //don't need to read from its output

            if(dup2(shell_in[READ], STDIN_FILENO) == -1 ||
                dup2(shell_out[WRITE], STDOUT_FILENO) == -1 ||
                dup2(shell_out[WRITE], STDERR_FILENO) == -1) {
                fprintf(stderr, "%s: error duping pipe fds: %s\n", argv[0], strerror(errno));
                tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
                exit(1);
            }

            if(execl(shell, shell, (char*) NULL) == -1){
                fprintf(stderr, "%s: error executing %s: %s\n", argv[0], shell, strerror(errno));
                tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
                exit(1);
            }
        }
        else {
            close(shell_in[READ]); //same as above
            close(shell_out[WRITE]);

            struct pollfd fds[2];

            fds[KEYBOARD].fd = STDIN_FILENO;
            fds[KEYBOARD].events = POLLIN;
            fds[SHELL_OUT].fd = shell_out[READ];
            fds[SHELL_OUT].events = POLLIN;

            signal(SIGPIPE, SIG_IGN); //don't want to exit/crash from writing to closed pipe

            int poll_ret, bytes_read, status_flag = 0; //status_flag is EOF indicator
            char *kb_buffer, *shell_buffer;
            kb_buffer = malloc(16);
            shell_buffer = malloc(256);
            while (1) {
                poll_ret = poll(fds, 2, 0);

                if (poll_ret == -1) {
                    fprintf(stderr, "%s: error in poll: %s\n", argv[0], strerror(errno));
                    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
                    exit(1);
                } else if (poll_ret > 0) {
                    if (fds[KEYBOARD].revents & POLLIN) {
                        bytes_read = read(STDIN_FILENO, kb_buffer, 16);
                        for (int i = 0; i < bytes_read; ++i) {
                            if((status_flag = write_input(shell_in[WRITE], kb_buffer[i]))){
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
                    }
                    if (fds[SHELL_OUT].revents & POLLIN) {
                        bytes_read = read(shell_out[READ], shell_buffer, 256);
                        for (int i = 0; i < bytes_read; ++i) {
                            if(write_input(-1, shell_buffer[i]) == 1){ //EOF
                                close(shell_out[READ]);
                                break;
                            }
                        }
                    } else if(fds[SHELL_OUT].revents & POLLHUP) { //still want to finish reading whatever is left
                        int wstatus;

                        if((waitpid(pid, &wstatus, WUNTRACED) == -1)) {
                            fprintf(stderr, "%s: waitpid syscall error: %s\n", argv[0], strerror(errno));
                            tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
                            exit(1);
                        }

                        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
                        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n",
                                WTERMSIG(wstatus), WEXITSTATUS(wstatus));
                        exit(0);
                    }
                }
            }
        }
    }
    else {
        char *buffer;
        int bytes_read, status_flag = 0; //EOF flag
        buffer = malloc(16);
        while (!status_flag && (bytes_read = read(0, buffer, 16))) {
            for (int i = 0; i < bytes_read; ++i) {
                status_flag = write_input(-1, buffer[i]);
                if (status_flag) break; //want to exit both loops when EOF is discovered
            }
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig); //restore original settings
}