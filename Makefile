CC=g++
CFLAGS= -g -pthread -lssl -Wall -Werror

all: proxy

proxy: proxy.o proxy_helpers.o proxy_cache.o
	$(CC) $(CFLAGS) -o proxy proxy.o proxy_cache.o proxy_helpers.o

proxy.o: proxy.c proxy_helpers.h proxy_cache.h
	$(CC) $(CFLAGS) -o proxy.o -c proxy.c

proxy_helpers.o: proxy_helpers.c proxy_helpers.h
	$(CC) $(FLAGS) -o proxy_helpers.o -c proxy_helpers.c

proxy_cache.o: proxy_cache.c
	$(CC) $(FLAGS) -g -o proxy_cache.o -c proxy_cache.c
clean:
	rm -f proxy *.o

tar:
	tar -cvzf cos461_ass3_$(USER).tgz proxy.c proxy_cache.c proxy_cache.h proxy_helpers.c proxy_helpers.h README Makefile
