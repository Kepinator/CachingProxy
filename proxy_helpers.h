

#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netdb.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include <unistd.h>


#define DEFAULT_HTTP_PORT "80"
#define DEFAULT_METHOD "GET "
#define DEFAULT_VERSION " HTTP/1.0"  
#define MAX_QUEUE 100
#define MAX_NUM_THREADS 100
#define BUF_START_SIZE 256
#define MAX_BUF_SIZE 65536    /*64 kb*/
#define CRLF "\r\n"
#define DUBCRLF "\r\n\r\n"

#define HTTP_ERR_400 "HTTP/1.0 400 Bad Request\r\n\r\n"
#define HTTP_ERR_500 "HTTP/1.0 500 Internal Server Error\r\n\r\n"
#define HTTP_ERR_501 "HTTP/1.0 501 Not Implemented\r\n\r\n" 
#define HTTP_ERR_502 "HTTP/1.0 502 Bad Gateway\r\n\r\n"


enum {FALSE, TRUE};

/* a function to call send() in a loop with error checking. returns
   the total number of bytes sent or -1 on error */
int Send(int sockfd, const void *msg, int len, int flags);

/* Places the first 3 characters of the method into memory designated by method 
   Determines if request is a GET request, returns 1 if TRUE, 0 otherwise */ 
int parse_method(void *buf, char *method);

/* Parse the URL from header, allocate memory and store url,
   return a pointer to url or NULL if allocation fails or there
   is no url field in the header (invalid header) */
char *parse_url(void *buf, size_t bufsize);

/* Places the first 8 characters of the version into memory designated by http_version.
   Determines if this string is a valid HTTP version, returning 1 if TRUE, 0 otherwise */
int parse_http_version(void *buf, size_t bufsize, char *http_version);

/* Parses host and path apart, putting the path into url, and returning a pointer 
   to host, or NULL if there is no host, or if memory allocation fails */ 
char *parse_host(char *url, void *buf, size_t bufsize);	

/* Return the port number if included in host header, 0 otherwise */
char *parse_port(char *host);

/* Perform cleanup on http request from client and send it
   out over s_server. Return 1 on successful send, 0 otherwise */ 
int send_http_request(int s_server, char *host, char *url, void *buf, size_t *bufsize, int length);

/* Check if buf is terminated by a \r\n\r\n, return 1 if TRUE, 0 otherwise */
int hasdoublecrlf(void * buf);

/* send msg over socket, then close socket */
void send_err(int socket, char *msg);

