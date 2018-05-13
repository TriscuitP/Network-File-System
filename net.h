#ifndef _NET_H_
#define _NET_H_

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

enum msg_types {
    MSG_READDIR = 1, 
    MSG_GETATTR = 2,
    MSG_OPEN = 3,
    MSG_READ = 4
};

struct __attribute__((__packed__)) netfs_msg_header {
    uint64_t msg_len;
    uint16_t msg_type;
};

struct attr_stat
{
	ino_t ino;	            /* Inode number */
    uid_t uid;              /* User ID of owner */
    gid_t gid;              /* Group ID of owner */
    mode_t mode;            /* File type and mode */
    nlink_t nlink;          /* Number of hard links */
    off_t size;             /* Total size, in bytes */
    blkcnt_t blocks;        /* Number of 512B blocks allocated */
    struct timespec mtim;   /* Time of last modification */
};

int connect_to(char *hostname, int port);
ssize_t read_len(int fd, void *buf, size_t length);
ssize_t write_len(int fd, void *buf, size_t length);

#endif
