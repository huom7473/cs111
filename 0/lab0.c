/* NAME: Michael Huo */
/* EMAIL: EMAIL */
/* ID: UID */
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

void handle_segfault() {
  fprintf(stderr, "Caught segfault\n");
  exit(4);
}

void cause_segfault() {
  char* p = 0;
  *p = 's';
}

int main(int argc, char** argv) {
  int c;
  char* input = 0;
  char* output = 0;
  bool segfault = 0;
  bool catch = 0;
  static struct option long_options[] =
  {
   {"input", required_argument, 0, 'i'},
   {"output", required_argument, 0, 'o'},
   {"segfault", no_argument, 0, 's'},
   {"catch", no_argument, 0, 'c'}
  };
  while ((c = getopt_long(argc, argv, "", long_options, 0)) != -1) {
    switch(c) {
    case 'i':
      input = optarg;
      break;
    case 'o':
      output = optarg;
      break;
    case 's':
      segfault = 1;
      break;
    case 'c':
      catch = 1;
      break;
    default: //bad arg, error message should print from getopt_long
      fprintf(stderr, "usage: %s [--input=<filename>] [--output=<filename>] [--segfault] [--catch]\n", argv[0]);
      exit(1); //unrecognized argument exit code
    }
  }

  if(input) {
    int ifd = open(input, O_RDONLY);
    if (ifd > 0) {
	close(0);
	dup(ifd);
	close(ifd);
    }
    else {
      //fprintf(stderr, "errno: %d\n", errno);
      fprintf(stderr, "%s: --input: error opening %s: %s\n", argv[0], input, strerror(errno));
      exit(2);
    }
  }

  if(output) {
   int ofd = creat(output, 0666);
   if (ofd >= 0) {
  	close(1);
  	dup(ofd);
  	close(ofd);
   }
   else {
     fprintf(stderr, "%s: --output: error writing %s: %s\n", argv[0], output, strerror(errno));
     exit(3);
   }
  }

  if(catch)
    signal(SIGSEGV, handle_segfault);

  if(segfault)
    cause_segfault();

  void* buffer;
  int bytes_written;
  buffer = malloc(64);
  // if(!buffer) exit(1); //we have big issues
  while ((bytes_written = read(0, buffer, 64))) {
    write(1, buffer, bytes_written);
  }
  
  return 0;
}

