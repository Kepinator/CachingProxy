#include "proxy_helpers.h"
#include <openssl/sha.h>
#include <regex.h>
#define CACHE_DIRECTORY ".proxy_cache"

struct cacheTree
{
	char hash[SHA_DIGEST_LENGTH];
	struct cacheTree *lt;
	struct cacheTree *gt;
};

static void cacheTree_free(struct cacheTree *root)
{
	if (root->lt != NULL)
		cacheTree_free(root->lt);
	if (root->gt != NULL)
		cacheTree_free(root->gt);
	free(root);
}

static int cacheTree_contains(char *url, struct cacheTree *index)
{
	int diff;
	char URLhash[SHA_DIGEST_LENGTH];
	SHA1((unsigned char *)url, strlen(url), (unsigned char *)URLhash);

	while (index != NULL)
	{
		diff = strncmp(URLhash, index->hash, SHA_DIGEST_LENGTH);
		if (diff == 0)
			return 1;
		else if (diff < 0)
			index = index->lt;
		else if (diff > 0)
			index = index->gt;
	}
	return 0;
}

static int cacheTree_add(char *url, struct cacheTree *index)
{
	int diff;
	struct cacheTree *prevIndex;
	char URLhash[SHA_DIGEST_LENGTH];
	SHA1((unsigned char *)url, strlen(url), (unsigned char *)URLhash);

	if (index == NULL)
		return -1;

	while (index != NULL)
	{
		diff = strncmp(URLhash, index->hash, SHA_DIGEST_LENGTH);
		if (diff == 0)
			return 1;
		else if (diff < 0)
		{
			prevIndex = index;
			index = index->lt;
		}
		else if (diff > 0)
		{
			prevIndex = index;
			index = index->gt;
		}
	}
	if (strncmp(URLhash, prevIndex->hash, SHA_DIGEST_LENGTH) < 0)
	{
		prevIndex->lt = (struct cacheTree *)malloc(sizeof(struct cacheTree));
		if (prevIndex->lt == NULL)
			return -1;

		prevIndex->lt->lt = NULL;
		prevIndex->lt->gt = NULL;
		strncpy(prevIndex->lt->hash, URLhash, SHA_DIGEST_LENGTH);
		return 1;
	}
	else
	{
		prevIndex->gt = (struct cacheTree *)malloc(sizeof(struct cacheTree));
		if (prevIndex->gt == NULL)
			return -1;

		prevIndex->gt->lt = NULL;
		prevIndex->gt->gt = NULL;
		strncpy(prevIndex->gt->hash, URLhash, SHA_DIGEST_LENGTH);
		return 1;
	}
}

/* If buf contains a valid no-cache header or private header, return 1, else return 0. */ 
static int no_cache(void * buf, size_t bufsize)
{
	regex_t re;
	int success;
	char *pattern = "(Cache-Control\\s*:\\s*(private|no-cache))|(Pragma\\s*:\\s*no-cache)";
	if (regcomp(&re, pattern, REG_EXTENDED) != 0) 
		return -1;
	success = regexec(&re, (char *)buf, (size_t)0, NULL, 0);
	regfree(&re);
	if (success == 0)
		return 1;
	else if (success == REG_NOMATCH)
		return 0;
	else
		return -1;
}


/* create new cache index and return a pointer to it or NULL on failure*/
struct cacheTree *proxycache_create(void)
{
	/* directory to store cached pages in
	   will automatically fail if directory has already been created */
   mkdir(CACHE_DIRECTORY, S_IRWXU);

	struct cacheTree *index;
	if ( (index = (struct cacheTree *)malloc(sizeof(struct cacheTree))) == NULL)
		return NULL;
	bzero(index->hash, SHA_DIGEST_LENGTH);
	index->lt = NULL;
	index->gt = NULL;
	return index;
}

/* return 1 if url composed of host ^ path is in cache index, 0 otherwise */ 
int proxycache_iscached(char *host, char *path, struct cacheTree *index)
{
	int contains;
	char *url = (char *)malloc(strlen(host) + strlen(path) + 1);
	url[0] = '\0';
	strcat(url, host);
	strcat(url, path);
	contains = cacheTree_contains(url, index);
	free(url);
	return contains;
}

/* send the cached page over sock */
void proxycache_returncached(char *host, char *path, int sock, void *buf, size_t bufsize)
{
	int recieved;
	FILE *cached_file;
	char *filepath = (char *)malloc(strlen(CACHE_DIRECTORY) + SHA_DIGEST_LENGTH + 2);
	char *url = (char *)malloc(strlen(host) + strlen(path) + 1);
	
	/* construct url */
	url[0] = '\0';
	strcat(url, host);
	strcat(url, path);
	char URLhash[SHA_DIGEST_LENGTH]; 
	SHA1((unsigned char *)url, strlen(url), (unsigned char *)URLhash);

	/* construct file path */
	filepath[0] = '\0';
	strcat(filepath, CACHE_DIRECTORY);
	strcat(filepath, "/");
	strncat(filepath, URLhash, SHA_DIGEST_LENGTH);

	/* open cache file and send it over socket sock */
	cached_file = fopen(filepath, "r");
	while ( (recieved = fread(buf, 1, bufsize, cached_file)) > 0)
		Send(sock, buf, recieved, 0);
	fclose(cached_file);
	free(url);
	free(filepath);
}

/* if private or no-cache not specified, 
   write the page at url host + path to a cache file, and add it to the cache index. Return
-1 on error, 0 if page is cached, or 1 if no-cache  */
int proxycache_addtocache(char *host, char *path, int s_server,int s_client, void *buf, size_t *bufsize,
							struct cacheTree *index)
{
	int length=0, available = *bufsize; 
	int recieved;
	FILE *cache_file;
	char *filepath = (char *)malloc(strlen(CACHE_DIRECTORY) + SHA_DIGEST_LENGTH + 2);
	char *url = (char *)malloc(strlen(host) + strlen(path) + 1);
	
	/* construct url */
	url[0] = '\0';
	strcat(url, host);
	strcat(url, path);
	char URLhash[SHA_DIGEST_LENGTH]; 
	SHA1((unsigned char *)url, strlen(url), (unsigned char *)URLhash);

	/* construct file path */
	filepath[0] = '\0';
	strcat(filepath, CACHE_DIRECTORY);
	strcat(filepath, "/");
	strncat(filepath, URLhash, SHA_DIGEST_LENGTH);
	/* recieve all headers (some message body may be recieved, but is irrelevant.
	   search for a properly formed non-cache or private header, and if found, 
	   proceed with recving and sending to client, then return 1  */
	while ( (recieved = recv(s_server,((char *)buf+length), available, 0)))
	{
		available -= recieved;
		length += recieved;

		if (hasdoublecrlf(buf))
			break;

		if (available == 0)
		{
			if (*bufsize < MAX_BUF_SIZE)
				*bufsize = *bufsize*2;
			if ( (buf = realloc(buf, *bufsize+1)) == NULL)
			{
				free(url);
				free(filepath);
				return -1;
			}
			available = *bufsize/2;
		}	
	}
	if (no_cache(buf, *bufsize))
	{
		Send(s_client, buf, length, 0);
		while ( (recieved = recv(s_server, buf, *bufsize, 0)) != 0)
			Send(s_client, buf, recieved, 0);

		close(s_server);
		close(s_client);
		free(buf);
		free(url);
		free(filepath);
		return 1;
	}
	
	cache_file = fopen(filepath, "w");
	if (cache_file == NULL)
		return -1;

	/* write to cache */
	fwrite(buf, 1, length, cache_file);
	while ( (recieved = recv(s_server, buf, *bufsize, 0)))
		fwrite(buf, 1, recieved, cache_file);
								
	cacheTree_add(url, index);
	fclose(cache_file);
	free(url);
	free(filepath);
	free(buf);
		return 0;
}





