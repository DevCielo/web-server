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

/* structures */
struct sHttpRequest
{
 char method[8]; /* stores the HTTP method (e.g. GET, POST) */
 char url[128]; /* stores the URL from the request line */
};
typedef struct sHttpRequest httpreq;

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

/* returns 0 on error, or it returns a httpreq structure */
/* parses the HTTP request string (buff) into a structured httpreq format */
httpreq *parse_http(char *str) 
{
 httpreq *req;
 char *p;

 req = malloc(sizeof(httpreq));

 for (p=str; *p && *p != ' '; p++); /* itrates through the template to find the first space (the HTTP method is the code before the space) */
 if (*p == ' ')
  *p = 0; /* if found it is replaced with a null terminator to isolate the method */
 else
 {
  error = "parse_http() NOSPACE error";
  free(req);

  return 0;
 }

 strncpy(req->method, str, 7); /* copies up to 7 characters from the method into httpreq method field */

 for (str=++p; *p && *p != ' '; p++);
 if (*p == ' ')
    *p = 0;
 else
 {
    error = "parse_http() 2NDSPACE error";
    free(req);
    return 0;
 }

 strncpy(req->url, str, 127); /* copies up to 127 characters from the method into the httpreq url field */
 return req;
}

/* return 0 on error, or return the data */
char *cli_read(int c)
{
 static char buf[512];

 memset(buf, 0, 512);
 if (read(c, buf, 511) < 0)
 {
  error = "read() error";
  return 0;
 }
 else
  return buf;
}

/* Sends the actual HTTP response body to the client (in HTML) */
void http_response(int c, char *contenttype, char *data)
{
 char buf[512];
 int n;

 n = strlen(data);
 memset(buf, 0, 512);
 snprintf(buf, 511,
   "Content-Type: %s\n" // Content type
   "Content-Length: %d\n" // Content Length for the body
   "\n%s\n", // Response data (body)
   contenttype, n, data);

 n = strlen(buf);
 write(c, buf, n);

 return;
}

/* Generates and sends the HTTP headers to the client (providing information about the response including status code, content language etc. */
void http_headers(int c, int code)
{
 char buf[512]; // Buffer to hold header string
 int n; // Number of bytes written

 memset(buf, 0, 512);
 snprintf(buf, 511,
   "HTTP/1.0 %d OK\n" // HTTP version and status code
   "Server: httpd.c\n" // Server name
   "Cache-Control: no-store, no-cache, max-age=0, private\n" // Caching policies
   "Content-Language: en\n" // Content language set to English
   "Expires: -1\n" // Prevents caching
   "X-Frame-Options: SAMEORIGIN\n", // Security header
   code); // Status code passed (e.g. 200, 404 etc.)

 n = strlen(buf);
 write(c, buf, n); // Sends the header string to the client

 return;
}

void cli_conn(int s, int c)
{
 httpreq *req;
 char *p;
 char *res;

 p = cli_read(c); /* uses cli_read to read a section of the code */
 if (!p)
 {
  fprintf(stderr, "%s\n", error);
  close(c);
  return;
 }
 
 req = parse_http(p);
 if (!req)
 {
  fprintf(stderr, "%s\n", error);
  close(c);

  return;
 }

 /* if the request is GET, or matches "/app/webpage" Hello world html page is sent with success code */
 if (!strcmp(req->method, "GET") && !strcmp(req->url, "/app/webpage"))
 {
  res = "<html>Hello world</html>";
  http_headers(c, 200); /* 200 = everythings okay */
  http_response(c, "text/html", res);
 }
 else 
 {
  res = "File not found";
  http_headers(c, 404); /* 404 = file not found */
  http_response(c, "text/plain", res);
 }

 free(req);
 close(c);

 return;
}

int main(int argc, char *argv[])
{
 int s,c; /* server's and clients socket file descriptor */
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
