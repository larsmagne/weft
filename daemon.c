#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <getopt.h>

#include "config.h"
#include "transform.h"
#include "formatters.h"
#include "weft.h"

#define TRUE 1

union sock {
  struct sockaddr s;
  struct sockaddr_in i;
};

void closedown(int);

#define BUFFER_SIZE 4096

int server_socket = 0;
static int port = 8014;

int main(int argc, char **argv) {
  int wsd;
  int addlen;
  char *s;
  char buffer[BUFFER_SIZE];
  char *expression[BUFFER_SIZE];
  struct sockaddr_in sin, caddr;
  int nitems = 0;
  static int so_reuseaddr = TRUE;
  int dirn, i;
  char *command;
  time_t start_time;
  FILE *client;
  char *file_name, *output_file_name;
  struct stat stat_buf;

  dirn = parse_args(argc, argv);

  if (signal(SIGHUP, closedown) == SIG_ERR) {
    perror("Signal");
    exit(1);
  }

  if (signal(SIGINT, closedown) == SIG_ERR) {
    perror("Signal");
    exit(1);
  }

  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("No socket");
    exit(1);
  }

  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, 
	     sizeof(so_reuseaddr));

  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(server_socket, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    perror("Bind");
    exit(1);
  }

  if (listen(server_socket, 120) == -1) {
    perror("Bad listen");
    exit(1);
  }

  time(&start_time);

  printf("Accepting...\n");

  while (TRUE) {
    nitems = 0;
    wsd = accept(server_socket, (struct sockaddr*)&caddr, &addlen);

    i = 0;
    while (read(wsd, buffer+i, 1) == 1 &&
	   *(buffer+i) != '\n' &&
	   i++ < BUFFER_SIZE)
      ;
    if (*(buffer+i) == '\n')
      *(buffer+i+1) = 0;

    if (*buffer == 0) 
      goto out;
    
    printf("Got %s", buffer);

    s = strtok(buffer, " \n");

    while (s && nitems < BUFFER_SIZE) {
      expression[nitems++] = s;
      s = strtok(NULL, " \n");
    }
    
    expression[nitems] = NULL;

    if (nitems >= 1) {
      client = fdopen(wsd, "r+");

      command = expression[0];
      if (!strcmp(command, "input") && nitems == 2) {
	file_name = expression[1];
	if (stat(file_name, &stat_buf) == -1) {
	  fprintf(client, "error\n"); 
	  goto out;
	}
	
	picon_number = 0;
	binary_number = 0;

	if (S_ISREG(stat_buf.st_mode)) {
	  output_file_name = get_cache_file_name(file_name);
	  ensure_directory(output_file_name);
	  transform_file(file_name, output_file_name);
	  free(output_file_name);
	} else if (!strcmp(command, "quit")) {
	  closedown(0);
	} 

	fclose(client);
      }
    }

  out:
    close(wsd);

  }

  exit(1);
}

void closedown(int i) {
 time_t now = time(NULL);

 if (server_socket)
   close(server_socket);

 printf("Closed down at %s", ctime(&now));
 exit(0);
}
