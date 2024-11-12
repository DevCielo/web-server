/* httpd.c */
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* global */
char *error;

#define LISTENADDR "127.0.0.1" /* localhost */

/* returns 0 on error, else it returns a socket fd */
int srv_init(int portno)
{
 int s;
 struct sockaddr_in srv; /* server socket */

 s = socket(AF_INET, SOCK_STREAM, 0); /* communicates over the IPv4 domain with a TCP stream socket */
 if (s < 0)
 {
  error = "socket() error";
  return 0;
 }

 srv.sin_family = AF_INET; /* sin_family = address family essentially ensures server is configured to communicate over IPv4 */
 srv.sin_addr.s_addr = inet_addr(LISTENADDR); /*sin_addr.s_addr = IP address server will listen to (inet_addr converts to binary format) */
 srv.sin_port = htons(portno); /* sin_port = port number on which server will listen for incoming connections */

 /* bind associates socket with a specified ip address and port */
 if (bind(s, (struct sockaddr *)&srv, sizeof(srv)))
 {
  close(s);
  error = "bind() error";
  return 0;
 }

 /* after binding we listen for connections) */
 if (listen(s, 5))
 {
  close(s);
  error = "listen() error";
  return 0;
 }

 return s;
}

/* returns 0 on error, or returns the new client's socket fd */
int cli_accept(int s)
{
 int c;
 socklen_t addrlen; /* size of the client's address structure */
 struct sockaddr_in cli; /* stores ip address and port number of client's address */

 addrlen = 0;
 memset(&cli, 0, sizeof(cli));
 c = accept(s, (struct sockaddr *)&cli, &addrlen); /* accepts incoming connection, fills cli  with details about ip/port, addrlen is set to size of clients address */
 if (c < 0)
 {
  error = "accept() error";
  return 0;
 }

 return c;
}

void cli_conn(int s, int c)
{
 return;
}

int main(int argc, char *argv[])
{
 int s,c ; /* server's and clients socket file descriptor */
 char *port; /* holds the port number where the server will listen for connections */

 /* checks if user provided a port number when running program */
 /* e.g. './httpd 8080' is two arguments argv[0] is the ./httpd and argv[1] is the port number */
 if (argc < 2) /* argc is the count of command-line arguments. argv is an array of the arguments */
 {
  fprintf(stderr, "Usage: %s <listening port>\n", 
    argv[0]);
  return -1;
 }
 else
  port = argv[1]; 


 s = srv_init(atoi(port)); /* atoi converts to int */
 if (!s)
 {
  fprintf(stderr, "%s\n", error);
  return -1;
 }

 printf("Listening on %s:%s\n", LISTENADDR, port);

 /* allows the server to continuously listen for and handle incoming client connections */
 while(1)
 {
  c = cli_accept(s); /* cli_accept accepts a new incoming client connection. each client connection gets its own socket. */
  if (!c)
  {
   fprintf(stderr, "%s\n", error);
   continue;
  }

  printf("Incoming connection\n");

  /* parent process (main server loop) is responsible for listening for new connections while child processes handle each client */
  if (!fork()) /* creates a new process for handling each client */
   cli_conn(s, c); /* comes only if its the child process */
 }

 return -1;
}
