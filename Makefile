CFLAGS += -Wall -g -I/usr/include/fuse3 -lpthread -lfuse3 -D_FILE_OFFSET_BITS=64
LDFLAGS +=

all: netfs_client netfs_server

netfs_client: netfs_client.o net.o 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ 

netfs_server: netfs_server.o net.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ 

net.o: net.c net.h logging.h
netfs_client.o: netfs_client.c common.h logging.h
netfs_server.o: netfs_server.c common.h logging.h

clean:
	rm -f netfs_client netfs_server