

#include "proxy_helpers.h"

/* a function to call send() in a loop with error checking. returns
   the total number of bytes sent or -1 on error */
int Send(int sockfd, const void *msg, int len, int flags)
{
	/*
	int status, sent = 0;
	char *to_send = (char *)msg;

	while (sent != len)
	{   
		if ( (status = send(sockfd, to_send, len, flags)) < 0)
			    return status;
		else
		{
			sent += status;
			to_send = (char *)msg + sent;
		}   
	}   
	return sent;
	*/
	 int total = 0;        // how many bytes we've sent
	     int bytesleft = len; // how many we have left to send
		     int n;

			     while(total < len) {
					         n = send(sockfd,(char *) msg+total, bytesleft, 0);
							         if (n == -1) { break; }
									         total += n;
											         bytesleft -= n;
													     }

				     len = total; // return number actually sent here

					     return n==-1?-1:0; // return -1 on failure, 0 on success
}



/* Places the first 3 characters of the method into memory designated by method 
   Determines if request is a GET request, returns 1 if TRUE, 0 otherwise */ 
int parse_method(void *buf, char *method)
{
	int i;
	char *bufptr = (char *) buf;
	for (i = 0; i < 3; i++)
		method[i] = bufptr[i];
	
	method[3] = '\0';
	if (strcmp(method, "GET"))
		return 0;
	return 1;
}

/* Parse the URL from header, allocate memory and store url,
   return a pointer to url or NULL if allocation fails or there
   is no url field in the header (invalid header) */
   
char *parse_url(void *buf, size_t bufsize)
{
	size_t i, url_length=0;
	char *url;
	char *bufptr = (char *)buf;
	/* skip method, then skip any intervening whitespace */
	for (i = 0; !isspace(bufptr[i]) && i < bufsize; i++)
		;
	for ( ; isspace(bufptr[i]) && i < bufsize; i++)
		;
	for ( ; !isspace(bufptr[i]) && i < bufsize; i++)
		url_length++;
	if (url_length == 0)
		return NULL;
	i -= url_length;
	url = (char *)malloc(sizeof(char)*(url_length+1));
	if (url == NULL)
		return NULL;
	url_length = 0;
	
	for ( ; !isspace(bufptr[i]) && i <bufsize; i++) 
	{
		url[url_length++] = bufptr[i];
	}

	url[url_length] = '\0';
	return url;
}

/* Places the first 8 characters of the version into memory designated by http_version.
   Determines if this string is a valid HTTP version, returning 1 if TRUE, 0 otherwise */
int parse_http_version(void *buf, size_t bufsize, char *http_version)
{
	size_t i, j;
	char *bufptr = (char *)buf;
	/* skip method and url, then skip any intervening whitespace */
	for (i = 0; !isspace(bufptr[i]) && i < bufsize; i++)
		;
	for ( ; isspace(bufptr[i]) && i < bufsize; i++)
		;
	for ( ; !isspace(bufptr[i]) && i < bufsize; i++)
		;
	for ( ; isspace(bufptr[i]) && i < bufsize; i++)
		;
	for (j = 0; j < 8 && i < bufsize; j++)
		http_version[j] = bufptr[i++];
	http_version[j + 1] = '\0';
	if(strcmp(http_version, "HTTP/0.9") && strcmp(http_version, "HTTP/1.0") 
			&& strcmp(http_version, "HTTP/1.1"))
			return 0;
	return 1;
}

/* Parses host and path apart, putting the path into url, and returning a pointer 
   to host, or NULL if there is no host, or if memory allocation fails */ 
char *parse_host(char *url, void *buf, size_t bufsize)	
{
	int hashttp = FALSE;
	size_t i, j, host_length=0;
	char * host_hdr;
	char * header_ptr;
	char *bufptr = (char *)buf;
	/* if url is a full url, parse out host, place path at beginning
	   of array pointed to by url, and return host_hdr */
	if (strlen(url) >= strlen("HTTP://"))
		if (strcasestr(url, "HTTP://"))
		{
			url += strlen("HTTP://");
			hashttp = TRUE;
		}
	if (*url != '/')
	{
		for (i = 0 ; url[i] != '/' && i < strlen(url); i++)
			host_length++;

		host_hdr = (char *)malloc(sizeof(char)*(host_length+1));
	
		if (host_hdr == NULL)
			return NULL;
		host_length = 0;

		for (i = 0; url[i] != '/' && i < strlen(url); i++)
			host_hdr[host_length++] = url[i];
		host_hdr[host_length] = '\0';

		if (hashttp)
		{
			url -= strlen("HTTP://");
			i += strlen("HTTP://");
		}
		for (j = 0 ; i < strlen(url); i++)
			url[j++] = url[i];
		url[j] = '\0';
		if (strlen(url) == 0)
		{
			url[0] = '/';
			url[1] = '\0';
		}
		return host_hdr;
	}

	else if ( (header_ptr = strcasestr(bufptr, "HOST")) == NULL)
		return NULL;

	bufsize -= (header_ptr - bufptr);
	for (i = 0; header_ptr[i] != ':' && i < bufsize; i++)
		;
	i++;
	for ( ; isspace(header_ptr[i]) && i < bufsize; i++)
		;
	for ( ; !isspace(header_ptr[i]) && i < bufsize; i++)
		host_length++;

	i -= host_length;
	host_hdr = (char *)malloc(sizeof(char)*(host_length+1));
	if (host_hdr == NULL)
		return NULL;
	host_length = 0;

	for ( ; !isspace(header_ptr[i]) && i < bufsize; i++)
		host_hdr[host_length++] = header_ptr[i];
	host_hdr[host_length] = '\0';

	

	return host_hdr;
}

/* Return the port number if included in host header, 0 otherwise */
char *parse_port(char *host)
{
	char *port_ptr;

	if ( (port_ptr = strstr(host, ":") ) == NULL)
		return NULL;
	else
		return port_ptr;
}

/* Perform cleanup on http request from client and send it
   out over s_server. Return 1 on successful send, 0 otherwise */ 
int send_http_request(int s_server, char *host, char *url, void *buf, size_t *bufsize, int length)
{
	int i, j, write_offset;
	void *sendbuf = malloc(sizeof(char)*(*bufsize));
	char *sendbufptr = (char *)sendbuf;
	char *bufptr = (char *) buf;
	bufptr[0] = '\0';

	bzero(sendbuf, *bufsize);
	/* construct first line of header and host header*/
	strncat(sendbufptr, DEFAULT_METHOD, strlen(DEFAULT_METHOD));
	strncat(sendbufptr, url, strlen(url));
	strncat(sendbufptr, DEFAULT_VERSION, strlen(DEFAULT_VERSION));
	strncat(sendbufptr, CRLF, strlen(CRLF));
	strncat(sendbufptr, "Host: ", strlen("Host "));
	strncat(sendbufptr, host, strlen(host));
	strncat(sendbufptr, CRLF, strlen(CRLF));
				
	j = strlen(sendbufptr);
	for (i = 0; bufptr[i] != '\n'; i++)
		;
	i++;
	while (i < length)
	{
		while (i < length && j < *bufsize)
		{
			write_offset = 0;
			for ( ; bufptr[i] != '\r' && bufptr[i] != '\n' && i < length; i++)
			{
				sendbufptr[j++] = bufptr[i];
				write_offset++;
			}
			j -= write_offset;
			if (strlen((sendbufptr+j)) >= strlen("Host"))
			{
				if (strcasestr((const char *)sendbufptr + j, "Host"))
				{
					sendbufptr[j] = '\0';
					continue;
				}
			}
			if (strlen((sendbufptr+j)))
			{
				if (!strlen((sendbufptr+j)) || !strstr((const char *)sendbufptr + j, ":"))
				{
					sendbufptr[j] = '\0';
					continue;
				}
			}
				j += write_offset;

			if (bufptr[i] == '\r')
			{
				if (bufptr[i+1] == '\n')
					i += 2;
			}
			else if (bufptr[i] == '\n')
				i++;
			if (bufptr[i] == '\r' || bufptr[i] == '\n')
			{
				strncat(sendbufptr, DUBCRLF, strlen(DUBCRLF));
				if (bufptr[i] == '\r')
					i += 2;
				if (bufptr[i] == '\n')
					i++;
				break;
			}
			else
				strncat(sendbufptr, CRLF, strlen(CRLF));
		}
		if (j == *bufsize)
		{
				if (*bufsize < MAX_BUF_SIZE)
					*bufsize *= 2;
				else
					return 0;
				if ( (sendbuf = realloc(sendbuf, *bufsize+1)) == NULL)
					return 0;
				else 
					sendbufptr = (char *)sendbuf;
		}
	}
	if (!Send(s_server, sendbufptr, strlen(sendbufptr), 0))
	{	
		free(sendbuf);
		return 0;
	}
	free(sendbuf);
	return 1;
}
		
/* Check if buf is terminated by a \r\n\r\n, return 1 if TRUE, 0 otherwise */
int hasdoublecrlf(void * buf)
{
	if (strstr((char *)buf, DUBCRLF))
		return 1;
	if (strstr((char *)buf, "\n\n"))
		return 1;
	return 0;
}

/* send msg over socket, then close socket */
void send_err(int socket, char *msg)
{
	Send(socket, msg, strlen(msg), 0);
	close(socket);
}


