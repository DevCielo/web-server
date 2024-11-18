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
#include <fcntl.h> // Added for open and O_RDONLY

/* structures */
struct sHttpRequest
{
    char method[8];    /* stores the HTTP method (e.g. GET, POST) */
    char url[128];     /* stores the URL from the request line */
};
typedef struct sHttpRequest httpreq;

struct sFile {
    char filename[64];
    char *fc;          /* fc = file contents */
    int size;
};
typedef struct sFile File;

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
    srv.sin_addr.s_addr = inet_addr(LISTENADDR); /* sin_addr.s_addr = IP address server will listen to (inet_addr converts to binary format) */
    srv.sin_port = htons(portno); /* sin_port = port number on which server will listen for incoming connections */

    /* bind associates socket with a specified ip address and port */
    if (bind(s, (struct sockaddr *)&srv, sizeof(srv)))
    {
        close(s);
        error = "bind() error";
        return 0;
    }

    /* after binding we listen for connections */
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
    socklen_t addrlen = sizeof(struct sockaddr_in); /* size of the client's address structure */
    struct sockaddr_in cli; /* stores ip address and port number of client's address */

    memset(&cli, 0, sizeof(cli));
    c = accept(s, (struct sockaddr *)&cli, &addrlen); /* accepts incoming connection, fills cli with details about ip/port, addrlen is set to size of clients address */
    if (c < 0)
    {
        error = "accept() error";
        return 0;
    }

    return c;
}

/* returns 0 on error, or returns a httpreq structure */
/* parses the HTTP request string (str) into a structured httpreq format */
httpreq *parse_http(char *str) 
{
    httpreq *req;
    char *p;

    req = malloc(sizeof(httpreq));
    if (!req) {
        error = "malloc() error";
        return 0;
    }

    for (p = str; *p && *p != ' '; p++); /* iterates through the template to find the first space (the HTTP method is the code before the space) */
    if (*p == ' ')
        *p = 0; /* if found it is replaced with a null terminator to isolate the method */
    else
    {
        error = "parse_http() NOSPACE error";
        free(req);
        return 0;
    }

    strncpy(req->method, str, 7); /* copies up to 7 characters from the method into httpreq method field */
    req->method[7] = '\0'; /* Ensure null-termination */

    for (str = ++p; *p && *p != ' '; p++);
    if (*p == ' ')
        *p = 0;
    else
    {
        error = "parse_http() 2NDSPACE error";
        free(req);
        return 0;
    }

    strncpy(req->url, str, 127); /* copies up to 127 characters from the method into the httpreq url field */
    req->url[127] = '\0'; /* Ensure null-termination */
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
        "X-Frame-Options: SAMEORIGIN\n" // Security header
        "\n", // End of headers
        code); // Status code passed (e.g. 200, 404 etc.)

    n = strlen(buf);
    write(c, buf, n); // Sends the header string to the client

    return;
}

/* returns 0 on error, or a file structure */
File *readfile(char *filename)
{
    char buf[512];
    int n, x, fd;
    File *f;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return 0;

    f = malloc(sizeof(struct sFile));
    if (!f)
    {
        close(fd);
        return 0;
    }

    strncpy(f->filename, filename, 63);
    f->filename[63] = '\0'; // Ensure null-termination
    f->fc = malloc(512);
    if (!f->fc)
    {
        close(fd);
        free(f);
        return 0;
    }

    x = 0; // bytes read
    n = 0; // Initialize n
    while (1)
    {
        memset(buf, 0, 512);
        n = read(fd, buf, 512);
        if (n <= 0)
            break;
        // Ensure there is enough space
        char *temp = realloc(f->fc, x + n);
        if (!temp)
        {
            close(fd);
            free(f->fc);
            free(f);
            return 0;
        }
        f->fc = temp;
        strncpy(f->fc + x, buf, n);
        x += n;
    }

    f->size = x;
    close(fd);

    return f;
}

/* 1 = everything ok, 0 on error 
 * Sends the HTTP response body (file content) and some headers to client via a socket connection. 
 * */
int send_file_content(int c, char *contenttype, File *file) // Renamed from sendfile
{
    char buf[512]; // buffer for sending headers
    char *p; // pointer to traverse file content
    int n, x; // n = size of remaining data to send, x = number of bytes successfully written in each loop

    if (!file) // if no file return null
        return 0;

    // Send headers
    memset(buf, 0, 512);
    snprintf(buf, 511,
        "Content-Type: %s\n"
        "Content-Length: %d\n"
        "\n",
        contenttype, file->size);

    n = strlen(buf);
    if (write(c, buf, n) != n)
        return 0;

    // Send file content
    n = file->size; // n is initialized to total file size
    p = file->fc; // p points to the start of the file's content 
    while (n > 0)
    {
        int to_write = (n < 512) ? n : 512;
        x = write(c, p, to_write); // sends file contents in chunks of up to 512 bytes
        if (x < 1)
            return 0;

        n -= x; // reduces size by number of bytes successfully sent
        p += x; // moves pointer forward by x bytes in file content
    }

    return 1;
}

void cli_conn(int s, int c)
{
    httpreq *req;
    char *p;
    char *res;
    char str[96];
    File *f;

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
    
    /* TODO: improve security by checking for things like "../.." */
    if (!strcmp(req->method, "GET") && !strncmp(req->url, "/img/", 5))
    {
        memset(str, 0, 96);
        snprintf(str, 95, ".%s", req->url); 
        f = readfile(str); 
        if (!f)
        {
            res = "File not found";
            http_headers(c, 404);
            http_response(c, "text/plain", res);
        }
        else
        {
            http_headers(c, 200);
            if (!send_file_content(c, "image/png", f)) // Updated function name
            {
                res = "HTTP server error";
                http_response(c, "text/plain", res);
            }
            free(f->fc);
            free(f);
        }
    }
    else if (!strcmp(req->method, "GET") && !strcmp(req->url, "/app/webpage"))
    {
        res = "<html><img src='img/test.png' alt='image' /></html>";
        http_headers(c, 200); /* 200 = everything's okay */
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
    int s, c; /* server's and client's socket file descriptor */
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
            cli_conn(s, c); /* comes only if it's the child process */
    }

    return -1;
}

