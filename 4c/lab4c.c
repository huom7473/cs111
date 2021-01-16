#include <mraa.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>

#define R0 100000
#define B 4275
#define SBUFFER 1024 //size of input/output buffers

sig_atomic_t volatile run_flag = 1;


int main(int argc, char **argv) {
  int tls = 0; //whether to use SSL
  double period = 1; //seconds (default 1 sec)
  char *scale = "F", scalechar, *logfn = NULL; //default Fahrenheit
  char *period_endptr = ""; //for string parse error checking
  char *id = NULL, *host = NULL;
  int port;
  char *port_endptr = "";

  static struct option long_options[] = {
          {"scale",  required_argument, 0, 's'},
          {"period", required_argument, 0, 'p'},
          {"log",    required_argument, 0, 'l'},
          {"id",     required_argument, 0, 'i'},
          {"host",   required_argument, 0, 'h'},
          {0,        0,                 0, 0}
  };

  int c;
  while ((c = getopt_long(argc, argv, "", long_options, 0)) != -1) {
    switch (c) {
      case 's':
        scale = optarg; //set opt_yield accordingly later
        break;
      case 'p':
        period = strtod(optarg, &period_endptr);
        break;
      case 'l':
        logfn = optarg;
        break;
      case 'i':
        id = optarg;
        break;
      case 'h':
        host = optarg;
        break;
      default:
        fprintf(stderr, "usage: %s [--scale=<F|C>] [--period=<seconds>] --log=</path/to/logfile>"
                        " --host=<name/address> --id=<id #> <port #>\n", argv[0]);
        exit(1);
    }
  }

  if (argc - optind != 1) { //check if there's exactly one argument at the end
    fprintf(stderr, "%s: must specify port number\n", argv[0]);
    exit(1);
  }
  else {
    port = strtol(argv[optind], &port_endptr, 10);
    if (*port_endptr || port < 0 || port > 65535) {
      fprintf(stderr, "%s: must specify valid port in range [0, 65535]\n", argv[0]);
      exit(1);
    }
  }

  char *id_endptr;
  if (!id || strlen(id) != 9 || strtol(id, &id_endptr, 10) < 0 || *id_endptr) {
    fprintf(stderr, "%s: must specify valid 9 digit id number\n", argv[0]);
    exit(1);
  }

  if (!host) {
    fprintf(stderr, "%s: must specify hostname or address (--host=<host>)\n", argv[0]);
    exit(1);
  }

  if (strcmp(scale, "F") && strcmp(scale, "C")) {
    fprintf(stderr, "%s: --scale: must specify valid temperature option [F]ahrenheit or [C]elsius (default F)\n",
            argv[0]);
    exit(1);
  }
  scalechar = scale[0]; //use a character so that we don't segfault when trying to change it

  if (period < 0 || *period_endptr) {
    fprintf(stderr, "%s: --period: must specify valid period in seconds\n",
            argv[0]);
    exit(1);
  }

  int logfd = -1;
  if (logfn) {
    if ((logfd = open(logfn, O_CREAT | O_WRONLY | O_APPEND, 0666)) == -1) {
      fprintf(stderr, "%s: error opening logfile %s: %s\n", argv[0], logfn, strerror(errno));
      exit(1);
    }
  }
  else { //log filename not specified
    fprintf(stderr, "%s: must specify log filename (--log=<logfile>)\n",
            argv[0]);
    exit(1);
  }

  // end of argument parsing

  //set up TCP socket

  struct addrinfo *res = NULL;
  struct addrinfo hints = {0}; //0 initialize - we won't be using all the fields

  hints.ai_family = AF_INET; //ipv4

  int errcode;
  if ((errcode = getaddrinfo(host, NULL, &hints, &res))) {//gethostname(3) obsolete according to man pages
    fprintf(stderr, "%s: getaddrinfo error: %s\n", argv[0], gai_strerror(errcode));
    exit(2);
  }

  ((struct sockaddr_in *) res->ai_addr)->sin_port = htons(port);
  int sfd = socket(AF_INET, SOCK_STREAM, 0);

  if (connect(sfd, res->ai_addr, sizeof(struct sockaddr_in)) == -1) {
    printf("%s: error connecting to server\n", argv[0]);
    exit(2);
  }

  dup2(sfd, STDOUT_FILENO); //redirect std out/in to two-way socket
  dup2(sfd, STDIN_FILENO);

  SSL_CTX *sslCtx;
  SSL *sslClient;
  if (strstr(argv[0], "tls") != NULL) { //figure out if we're using SSL
    tls = 1; //SSL connection, otherwise use TCP

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    sslCtx = SSL_CTX_new(TLSv1_client_method());
    if (!sslCtx) {
      fprintf(stderr, "Unable to get SSL context\n");
      exit(2); //copied from TA slides
    }

    sslClient = SSL_new(sslCtx);
    if (!sslClient) {
      fprintf(stderr, "Unable to complete SSL setup\n");
      exit(2);
    }

    SSL_set_fd(sslClient, sfd);
    if (SSL_connect(sslClient) != 1) {
      fprintf(stderr, "Failed to make SSL connection\n");
      exit(2);
    }

    //fprintf(stderr, "Connected through SSL successfully\n");
  }

  int tempreading;
  double temp;

  mraa_aio_context tempsens;
  tempsens = mraa_aio_init(1); //AIN 0

  if (!tempsens) {
    fprintf(stderr, "%s: an mraa pin init failed - this happens if sensors aren't properly connected,"
                    " not connected to the right pins (AIN_0 and GPIO_50), or the program needs to "
                    "be run by superuser to properly initialize them. exiting.\n", argv[0]);
    exit(2);
  }

  struct pollfd inpfd = {STDIN_FILENO, POLLIN, 0};

  char timestr[20]; //buffer to load time string into with strftime
  char inputbuf[SBUFFER], commandbuf[SBUFFER], outputbuf[SBUFFER]; //buffers to store input, commands, output
  time_t t;
  struct tm *tmp;
  int poll_ret, bytes_read, bytes_written, current_byte = 0; //current byte is index of first available byte in input buffer
  int commandlen;
  int report = 1; //whether or not to generate reports
  double orig_period; //in case PERIOD=[invalid period] is entered
  int i = 0; //while loop iterator
  int first_iteration = 0; //whether first report has been generated

  bytes_written = snprintf(outputbuf, SBUFFER, "ID=%s\n", id); //send ID line
  if (tls) { //ssl write
    if (SSL_write(sslClient, outputbuf, bytes_written) != bytes_written) {
      fprintf(stderr, "%s: error writing report over SSL connection\n", argv[0]);
      exit(2);
    }
  }
  else { //tcp write
    if (write(STDOUT_FILENO, outputbuf, bytes_written) != bytes_written) {
      fprintf(stderr, "%s: error writing report to stdout file descriptor\n", argv[0]);
      exit(2);
    }
  }

  while (1) {
    do { //process commands before next output
      if (!first_iteration) {
        first_iteration = 1;
        break; //don't test for input before generating first report
      }
      poll_ret = poll(&inpfd, 1, 0);
      if (poll_ret == -1) {
        fprintf(stderr, "%s: error polling for input: %s\n", argv[0], strerror(errno));
        exit(2);
      }
      else if (poll_ret > 0) {
        if (inpfd.revents & POLLIN) {
          if (current_byte == SBUFFER) {
            fprintf(stderr, "%s: current command too long - exiting to prevent buffer overflow\n", argv[0]);
            exit(2);
          }
          if (tls)
            bytes_read = SSL_read(sslClient, inputbuf + current_byte, SBUFFER - current_byte);
          else
            bytes_read = read(STDIN_FILENO, inputbuf + current_byte, SBUFFER - current_byte);

          if (bytes_read <= 0) { //EOF/error
            run_flag = 0;
            break;
          }

          current_byte += bytes_read;
          //read enough bytes to (possibly) fill up buffer (it currently has no command in it)

          //i = 0;
          while (i < current_byte) { //process all the commands in the buffer
            if (inputbuf[i] == '\n') {
              memcpy(commandbuf, inputbuf, i); //interpret all characters not including \n as command
              commandlen = i;
              commandbuf[commandlen] = '\0'; //make the command a string, basically replacing '\n' with '\0'
              //set the current byte to reflect the command that will be taken
              //out of the buffer as well as the \n character
              //(we're going to shift everything left and overwrite the command)
              current_byte -= i + 1;
              //shift the buffer left, over the old command
              memmove(inputbuf, inputbuf + i + 1, current_byte);
              //move current_byte bytes because that's the amount of relevant
              //data left
              //memmove doesn't care about memory region overlap, memcpy does

              //so now we have the command in commandbuf with commandlen length
              //and the rest of the relevant bytes in inputbuf
              if (logfd >= 0) {
                fsync(logfd); //flush buffers so unbuffered write is in correct order
                if (write(logfd, commandbuf, commandlen) != commandlen
                    || write(logfd, "\n", 1) != 1) { //write command and newline to file
                  fprintf(stderr, "%s: error writing command to log file\n", argv[0]);
                  exit(2);
                }
              }

              if (strcmp(commandbuf, "SCALE=F") == 0 || strcmp(commandbuf, "SCALE=C") == 0) {
                scalechar = commandbuf[6]; //set scale to character after SCALE=
              }
              else if ((unsigned) commandlen > strlen("PERIOD=") &&
                       strncmp(commandbuf, "PERIOD=", strlen("PERIOD=")) == 0) {
                orig_period = period;
                period = strtod(commandbuf + 7, &period_endptr);
                if (period < 0 || *period_endptr) //check that a valid period was entered
                  period = orig_period; //if not, restore from backup period value
              }
              else if (strcmp(commandbuf, "STOP") == 0)
                report = 0;
              else if (strcmp(commandbuf, "START") == 0)
                report = 1;
              else if (strcmp(commandbuf, "OFF") == 0) {
                run_flag = 0;
                break; //don't really need to worry about the rest of the commands
              }
              i = 0; //start searching for commands from the beginning again
            }
            else
              ++i;
          }
        }
        else if (inpfd.revents & (POLLHUP | POLLERR)) { //no more input
          run_flag = 0;
          break;
        }
      }
    } while (poll_ret > 0 && run_flag); //keep reading from buffer

    t = time(NULL);
    tmp = localtime(&t); //can have this in an infinite loop since localtime statically allocates tm struct
    //time getting from man page example

    if (!tmp) {
      fprintf(stderr, "%s: error in localtime call\n", argv[0]);
      exit(2);
    }

    if (strftime(timestr, sizeof(timestr), "%H:%M:%S", tmp) == 0) {
      fprintf(stderr, "%s: error formatting time string\n", argv[0]);
      exit(2);
    }

    if (!run_flag) {
      //dprintf(STDOUT_FILENO, "%s SHUTDOWN\n", timestr);
      bytes_written = snprintf(outputbuf, SBUFFER, "%s SHUTDOWN\n", timestr);
      if (tls) { //ssl write
        if (SSL_write(sslClient, outputbuf, bytes_written) != bytes_written) {
          fprintf(stderr, "%s: error writing report over SSL connection\n", argv[0]);
          exit(2);
        }
      }
      else { //tcp write
        if (write(STDOUT_FILENO, outputbuf, bytes_written) != bytes_written) {
          fprintf(stderr, "%s: error writing report to stdout file descriptor\n", argv[0]);
          exit(2);
        }
      }

      if (logfd >= 0)
        dprintf(logfd, "%s SHUTDOWN\n", timestr);

      mraa_aio_close(tempsens);

      if (tls) {
        SSL_shutdown(sslClient);
        SSL_free(sslClient);
      }

      exit(0);
    }

    if (report) {
      tempreading = mraa_aio_read(tempsens);

      double R = 1023.0 / tempreading - 1.0; //copying the formula on the temperature sensor tutorial
      R *= R0;
      temp = 1.0 / (log(R / R0) / B + 1 / 298.15) - 273.15;

      if (scalechar == 'F')
        temp = temp * 9 / 5 + 32;

      bytes_written = snprintf(outputbuf, SBUFFER, "%s %.1lf\n", timestr, temp);
      if (tls) { //ssl write
        if (SSL_write(sslClient, outputbuf, bytes_written) != bytes_written) {
          fprintf(stderr, "%s: error writing report over SSL connection\n", argv[0]);
          exit(2);
        }
      }
      else { //tcp write
        if (write(STDOUT_FILENO, outputbuf, bytes_written) != bytes_written) {
          fprintf(stderr, "%s: error writing report to stdout file descriptor\n", argv[0]);
          exit(2);
        }
      }

      if (logfd >= 0)
        dprintf(logfd, "%s %.1lf\n", timestr, temp);
    }

    usleep(1000000 * period); //convert to microseconds (10^6)
  }

  //really shouldn't get here, but just in case...
  mraa_aio_close(tempsens);

  if (tls) {
    SSL_shutdown(sslClient);
    SSL_free(sslClient);
  }
  return 0;
}
