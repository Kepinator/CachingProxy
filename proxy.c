

#include "proxy_helpers.h"		/* headers, defines, and helper functions for parsing, error reporting, etc. */ 
#include "proxy_cache.h"		/* ADT for a cache structure and operations */ 

pthread_mutex_t count_lock, cache_write_lock;
sem_t cache_read_sem;
int num_threads = 0;
cacheindex_T cache_index;

void *process_request(void *sock_id)
{
  char method[4];    /* strlen("GET")+1 */
  char *url;
  char http_version[9];		/* strlen("HTTP/X.X")+1 */
  char *host;
  char *serv_port;
  void *buf = malloc(BUF_START_SIZE+1);
  size_t bufsize = BUF_START_SIZE;
  int new_s_client =  (int) sock_id;
  int s_server;
  int length=0, i, recieved, available=BUF_START_SIZE;
  int GOT_CACHED;
  struct addrinfo hints, *servinfo, *p;


  bzero(buf, bufsize);

  /* update number of threads currently operating */
  pthread_mutex_lock(&count_lock);
  num_threads++;
  pthread_mutex_unlock(&count_lock);

  /* recieve request from client */
    length = 0;
	available = bufsize;
    while ( (recieved = recv(new_s_client, ((char *)buf+length), available, 0)))
	{
		available -= recieved;
		length += recieved;

		if (hasdoublecrlf(buf))
			break;

		if (available == 0)
		{
			if (bufsize < MAX_BUF_SIZE)
				bufsize *= 2;
			if ( (buf = realloc(buf, bufsize+1)) == NULL)
				break;
			available = bufsize/2;
		}
	}

	/* parse request */
	bzero(method, sizeof(method));
	bzero(http_version, sizeof(http_version));
	if (!parse_method(buf, method))
	{
		send_err(new_s_client, HTTP_ERR_501);
		return NULL;
	}

	if ( (url = parse_url(buf, bufsize)) == NULL)
	{
		send_err(new_s_client, HTTP_ERR_400);
		return NULL;
	}

	if (!parse_http_version(buf, bufsize, http_version))
	{
		send_err(new_s_client, HTTP_ERR_400);
		free(url);
		return NULL;
	}

	if ( (host = parse_host(url, buf, bufsize)) == NULL)
	{
		send_err(new_s_client, HTTP_ERR_400);
		free(url);
		return NULL;
	}

	if ( (serv_port = parse_port(host)) == NULL)
		serv_port = DEFAULT_HTTP_PORT;

	/* determine if page is currently cached */
	if (proxycache_iscached(host, url, cache_index))
	{
		pthread_mutex_lock(&cache_write_lock);
		sem_wait(&cache_read_sem);
		pthread_mutex_unlock(&cache_write_lock);
		proxycache_returncached(host, url, new_s_client, buf, bufsize);
		sem_post(&cache_read_sem);
		close(new_s_client);
		free(url);
		free(host);
		free(buf);

		pthread_mutex_lock(&count_lock);
		num_threads--;
		pthread_mutex_unlock(&count_lock);
		return NULL;
	}

	/* if not cached, send request to the specified host */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

	if ( (getaddrinfo(host, serv_port, &hints, &servinfo)) != 0) 
	{
		send_err(new_s_client, HTTP_ERR_502);
		free(url);
		free(host);
		free(buf);
		return NULL;
	}

	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		if ((s_server = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) < 0) 
			return NULL;
		if (connect(s_server, p->ai_addr, p->ai_addrlen) < 0) 
		{
			close(s_server);
			continue;
		}
		break;
	}
    freeaddrinfo(servinfo);

	if (p == NULL)
	{
		send_err(new_s_client, HTTP_ERR_500);
		free(url);
		free(host);
		free(buf);
		return NULL;
	}
	
	send_http_request(s_server, host, url, buf, &bufsize, length);	
	
	/* recieve response, and, if no-cache or private not specified, write webpage to cache. Send it to the client */
	pthread_mutex_lock(&cache_write_lock);
	for (i = 0; i < MAX_NUM_THREADS; i++)
		sem_wait(&cache_read_sem);

	if (proxycache_addtocache(host, url, s_server, new_s_client, buf, &bufsize, cache_index) == 0)
		GOT_CACHED = TRUE;
	else
		GOT_CACHED = FALSE;
	
	for (i = 0; i < MAX_NUM_THREADS; i++)
		sem_post(&cache_read_sem);
	pthread_mutex_unlock(&cache_write_lock);

	/* if page was cached, return page from cache */
	if (GOT_CACHED)
	{
		/* need to re-malloc buf, since it had to be freed in addtocache */
		buf =  malloc(BUF_START_SIZE+1);
		bufsize = BUF_START_SIZE;
			
		pthread_mutex_lock(&cache_write_lock);
		sem_wait(&cache_read_sem);
		pthread_mutex_unlock(&cache_write_lock);
		proxycache_returncached(host, url, new_s_client, buf, bufsize);
		sem_post(&cache_read_sem);
		close(s_server);
		close(new_s_client);
	}
	/* if not, send buffer, recieve remainder, and send it to client here */

	free(url);
	free(host);

	pthread_mutex_lock(&count_lock);
	num_threads--;
	pthread_mutex_unlock(&count_lock);
	return NULL;
}



int main(int argc, char * argv[])
{
  void (*ret)(int);
  struct sockaddr_in sin;
  int s_client;
  int new_s_client;
  socklen_t sockLen = sizeof(struct sockaddr);
  uint16_t portnum;
  pthread_t thread;	  
  pthread_attr_t thread_attr;	

  pthread_attr_init(&thread_attr);
  pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

  cache_index = proxycache_create();

  /* ignore SIGPIPE signals, only an issue for very large numbers of concurrent requests
	 (e.g. 4-5*MAX_NUM_THREADS) */
 
  ret = signal(SIGPIPE, SIG_IGN);
  if (ret == SIG_ERR)
  {
	  perror(argv[0]);
	  exit(1);
  }
  
  int MULTITHREADED = FALSE;
  if (argc == 2)	  
  	  portnum = (uint16_t)atoi(argv[1]);
  else if (argc == 3)
  {
	  if (strcmp(argv[1], "-t"))
	  {
		  fprintf(stderr, "usage: proxy [-t] <portnum>\n");
          exit(1);
	  }
	  portnum = (uint16_t)atoi(argv[2]);
	  MULTITHREADED = TRUE;
  }
  else
  {
	  fprintf(stderr, "usage: proxy [-t] <portnum>\n");
      exit(1);
  }
  
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(portnum); 

  /*  open a socket and attempt to bind to the given port */
  if ( (s_client = socket(PF_INET, SOCK_STREAM, 0)) < 0)
  {
      perror("error requesting socket");
      exit (1);
  }
  setsockopt(s_client, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

  if ( (bind(s_client, (struct sockaddr *)&sin, sizeof(sin))) < 0)
    {
    perror("cannot bind to socket");
      exit(1);
    }

  listen(s_client, MAX_QUEUE);
  
  pthread_mutex_init(&count_lock, NULL);
  pthread_mutex_init(&cache_write_lock, NULL);
  sem_init(&cache_read_sem, PTHREAD_PROCESS_PRIVATE, MAX_NUM_THREADS);

  while (TRUE)
  {
	 if (num_threads < MAX_NUM_THREADS)
	 {
		 if ( (new_s_client = accept(s_client, (struct sockaddr *)&sin, &sockLen)) < 0)
			continue;
		 /* if -t specified, create new thread for the new request, else handle sequentially */
		 if (MULTITHREADED)
		 {

		     if (pthread_create(&thread, &thread_attr, process_request, (void *)new_s_client) != 0)
		     {
			     send_err(new_s_client, HTTP_ERR_500);
			     close(new_s_client);
		     }
		 }
		 else
			 process_request((void *) new_s_client);
	 }
  } 
  return 1;
}

