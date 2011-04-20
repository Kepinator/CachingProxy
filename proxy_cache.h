/*******************************************************
  * A binary tree-based ADT for tracking and adding to the
  * cache.
  * http 1.0 requests
********************************************************/

#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct cacheTree *cacheindex_T;

/* create new cache index and return a pointer to it or NULL on failure*/
struct cacheTree *proxycache_create(void);

/* return 1 if url composed of host ^ path is in cache index, 0 otherwise */ 
int proxycache_iscached(char *host, char *path, struct cacheTree *index);

/* send the cached page over sock */
void proxycache_returncached(char *host, char *path, int sock, void *buf, size_t bufsize);

/* write the page at url host + path to a cache file, and add it to the cache index */
int proxycache_addtocache(char *host, char *path, int s_server,int s_client, void *buf, size_t *bufsize,
							struct cacheTree *index);

