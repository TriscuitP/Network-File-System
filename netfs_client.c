/**
 * netfs_client.h
 *
 * Implementation of the netfs client file system. Based on the fuse 'hello'
 * example here: https://github.com/libfuse/libfuse/blob/master/example/hello.c
 */

#define FUSE_USE_VERSION 31

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse3/fuse.h>
#include <netdb.h> 
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include "common.h"
#include "logging.h"
#include "net.h"

#define TEST_DATA "hello world!\n"

/* Command line options */
static struct options {
    int show_help;
    int port;
} options;

#define OPTION(t, p) { t, offsetof(struct options, p), 1 }

/* Command line option specification. We can add more here. If we're interested
 * in a string, specify --opt=%s .*/
static const struct fuse_opt option_spec[] = {
    OPTION("-h", show_help),
    OPTION("--help", show_help),
    OPTION("--port=%d", port),
    FUSE_OPT_END
};

// TODO: uids and gids
// TODO: read-only
static int netfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    LOG("getattr: %s\n", path);

    struct passwd *pw;
    uid_t uid;

    uid = geteuid();
    pw = getpwuid(uid);

    struct netfs_msg_header req_header = { 0 };
    req_header.msg_type = MSG_GETATTR;
    req_header.msg_len =  strlen(path) + 1;

    /* TODO: change localhost to what user puts */
    int server_fd = connect_to("localhost", DEFAULT_PORT);
    
    // We should check if server is less than 0 here...
    if(server_fd < 0)
    {
        perror("Socket failed");
        exit(-1);
    }

    LOG("server_fd: %d\n", server_fd);

    /* Clear the stat buffer */
    memset(stbuf, 0, sizeof(struct stat));

    /* By default, we will return 0 from this function (success) */
    int res = 0;

    struct attr_stat atst = { 0 };

    if(strcmp(path, "/") == 0) 
    {
        /* This is the root directory. We have hard-coded the permissions to 755
         * here, but you should apply the permissions from the remote directory
         * instead. The mode means:
         *   - S_IFDIR: this is a directory
         *   - 0755: user can read, write, execute. All others can read+execute.
         * The number of links refers to how many hard links point to the file.
         * If the link count reaches 0, the file is effectively deleted (this is
         * why deleting a file is actually 'unlinking' it).
         */ 

        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;

        write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));
        write_len(server_fd, path, req_header.msg_len);     

        // read_len(server_fd, atst, sizeof(struct attr_stat));

        
        // if(atst == NULL)
        // {
        //     atst->mode = S_IFDIR | 0755;
        //     atst->nlink = 2;
        // }

        // atst->uid = stbuf->st_uid;
        // atst->gid = stbuf->st_gid;
        // atst->mode = stbuf->st_mode;
        // atst->nlink = stbuf->st_nlink;
        // atst->size = stbuf->st_size;

        // stbuf->st_uid = atst->uid;
        // stbuf->st_gid = atst->gid;
        // stbuf->st_mode = atst->mode;
        // stbuf->st_nlink = atst->nlink;
        // stbuf->st_size = atst->size;

        // printf("Directory information for %s\n", path);
        // printf("---------------------------\n");
        // printf("File size: \t\t%lld bytes\n", atst->size);
        // printf("Number of hard links: \t%d\n", atst->nlink);
        // printf("File inode: \t\t%llu\n", atst->ino);
        // printf("File type and mode: \t\t%hu\n", atst->mode);
        // printf("File user id: \t\t%u\n", atst->uid);
        // printf("File group id: \t\t%u\n", atst->gid);
     
        // printf("File Permissions: \t");
        // printf( (S_ISDIR(atst->mode)) ? "d" : "-");
        // printf( (atst->mode & S_IRUSR) ? "r" : "-");
        // printf( (atst->mode & S_IWUSR) ? "w" : "-");
        // printf( (atst->mode & S_IXUSR) ? "x" : "-");
        // printf( (atst->mode & S_IRGRP) ? "r" : "-");
        // printf( (atst->mode & S_IWGRP) ? "w" : "-");
        // printf( (atst->mode & S_IXGRP) ? "x" : "-");
        // printf( (atst->mode & S_IROTH) ? "r" : "-");
        // printf( (atst->mode & S_IWOTH) ? "w" : "-");
        // printf( (atst->mode & S_IXOTH) ? "x" : "-");
        // printf("\n\n");

        // return res;
    } 
    else if((path + 1) != NULL)
    {
        LOG("%s\n", "FILE STUFF");
        /* Incrementing the path pointer by 1 will remove the '/' from the start
         * of the path. We're comparing it with a hard-coded file name.
         *   - S_IFREG: indicates a regular file
         * We also hard-code the size of this file based on its contents: 'hello
         * world!' */

        // stbuf->st_mode = S_IFREG | 0444;
        // stbuf->st_nlink = 1;
        // stbuf->st_size = MAXIMUM_PATH;

        write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));
        write_len(server_fd, path, req_header.msg_len);     

        read_len(server_fd, &atst, sizeof(struct attr_stat));

        // atst.uid = stbuf->st_uid;
        // atst.gid = stbuf->st_gid;
        // atst.mode = stbuf->st_mode;
        // atst.nlink = stbuf->st_nlink;
        // atst.size = stbuf->st_size;

        stbuf->st_ino = atst.ino;
        stbuf->st_uid = atst.uid;
        stbuf->st_gid = atst.gid;
        stbuf->st_mode = atst.mode;
        stbuf->st_nlink = atst.nlink;
        stbuf->st_size = atst.size;
        stbuf->st_blocks = atst.blocks;
        stbuf->st_mtim = atst.mtim;

        printf("Directory information for %s\n", path);
        printf("---------------------------\n");
        printf("File size: \t\t%lld bytes\n", atst.size);
        printf("Number of hard links: \t%d\n", atst.nlink);
        printf("File inode: \t\t%llu\n", atst.ino);
        printf("File type and mode: \t%hu\n", atst.mode);
        printf("File user id: \t\t%u\n", atst.uid);
        printf("File group id: \t\t%u\n", atst.gid);
     
        printf("File Permissions: \t");
        printf( (S_ISDIR(atst.mode)) ? "d" : "-");
        printf( (atst.mode & S_IRUSR) ? "r" : "-");
        printf( (atst.mode & S_IWUSR) ? "w" : "-");
        printf( (atst.mode & S_IXUSR) ? "x" : "-");
        printf( (atst.mode & S_IRGRP) ? "r" : "-");
        printf( (atst.mode & S_IWGRP) ? "w" : "-");
        printf( (atst.mode & S_IXGRP) ? "x" : "-");
        printf( (atst.mode & S_IROTH) ? "r" : "-");
        printf( (atst.mode & S_IWOTH) ? "w" : "-");
        printf( (atst.mode & S_IXOTH) ? "x" : "-");
        printf("\n\n");

        // if(pw->pw_uid == stbuf->st_uid)
        // {
        //     printf("This file is owned by the current user\n");
        // }
        // else
        // {
        //     if((stbuf->st_mode & S_IROTH) == 4)
        //         printf("This file is owned by someone else but *IS* readable by this process\n");
        //     else
        //         printf("This file is owned by someone else and is *NOT* readable by this process\n");
        // }

        // return res;

    }
    else
    {
        res = -ENOENT;
    }

    //  -ENOENT = 'no such file or directory' 
    // return -ENOENT;
    return res;

}

static int netfs_readdir(
        const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
        struct fuse_file_info *fi, enum fuse_readdir_flags flags) 
{

    LOG("readdir: %s\n", path);

    /* By default, we will return 0 from this function (success) */
    int res = 0;

    struct netfs_msg_header req_header = { 0 };
    req_header.msg_type = MSG_READDIR;
    req_header.msg_len =  strlen(path) + 1;

    LOG("msg_readdir: %d\n", MSG_READDIR);
    LOG("path: %s\n", path);

    /* TODO: change localhost to what user puts */
    int server_fd = connect_to("localhost", DEFAULT_PORT);
    
    // We should check if server is less than 0 here...
    if(server_fd < 0)
    {
        perror("Socket failed");
        exit(-1);
    }

    LOG("server_fd: %d\n", server_fd);

    write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));
    write_len(server_fd, path, req_header.msg_len);

    uint16_t reply_len;
    char reply_path[MAXIMUM_PATH] = { 0 };

    do {
        
        // filler(buf, ".", NULL, 0, 0);       // Current Directory
        // filler(buf, "..", NULL, 0, 0);       // Parent Directory
        printf("HERE\n");
        read_len(server_fd, &reply_len, sizeof(uint16_t));
        printf("HERE2\n");
        read_len(server_fd, reply_path, reply_len);
        printf("HERE3\n");

        printf("-> %s\n", reply_path);
        printf("reply_len: %d\n", reply_len);

        filler(buf, reply_path, NULL, 0, 0);

        

    } while(reply_len > 0);

    close(server_fd);
    /* We only support one directory: the root directory. */

    return res;
}

static int netfs_open(const char *path, struct fuse_file_info *fi) 
{

    LOG("open: %s\n", path);

    /* We only support opening the file in read-only mode */
    if((fi->flags & O_ACCMODE) != O_RDONLY) 
    {
        return -EACCES;
    }

    /* By default, we will return 0 from this function (success) */
    int res = 0;

    struct netfs_msg_header req_header = { 0 };
    req_header.msg_type = MSG_OPEN;
    req_header.msg_len =  strlen(path) + 1;

    /* TODO: change localhost to what user puts */
    int server_fd = connect_to("localhost", DEFAULT_PORT);
    
    // We should check if server is less than 0 here...
    if(server_fd < 0)
    {
        perror("Socket failed");
        exit(-1);
    }

    LOG("server_fd: %d\n", server_fd);

    write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));
    write_len(server_fd, path, req_header.msg_len);

    int fd;
    uint16_t success;

    read_len(server_fd, &success, sizeof(uint16_t));

    if(success == -1)
    {
        res = 1;
        printf("%s\n", "Open Failure");
    }
    else
    {
        printf("%s\n", "Open Successful");

        // Save file descriptor for read
        read_len(server_fd, &fd, sizeof(int));
        fi->fh = fd;
    }

    close(server_fd);
    return res;
}

static int netfs_read(
        const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) 
{

    LOG("read: %s\n", path);

    int fd = fi->fh;

    struct netfs_msg_header req_header = { 0 };
    req_header.msg_type = MSG_READ;
    req_header.msg_len =  strlen(path) + 1;

    /* TODO: change localhost to what user puts */
    int server_fd = connect_to("localhost", DEFAULT_PORT);
    
    // We should check if server is less than 0 here...
    if(server_fd < 0)
    {
        perror("Socket failed");
        exit(-1);
    }

    LOG("server_fd: %d\n", server_fd);
    LOG("*******offset: %lld\n", offset);
    LOG("*******offset: %d\n", offset);

    write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));

    write_len(server_fd, &fd, sizeof(int));

    write_len(server_fd, &size, sizeof(size_t));

    write_len(server_fd, &offset, sizeof(off_t));

    // for(size_t size_to_rec = size; size_to_rec > 0; )
    // {
    //     ssize_t rec = sendfile(fd, server_fd, &offset, size_to_rec);
    //     if(rec <= 0)
    //     {
    //         if(rec != 0)
    //             perror("sendfile");
    //         break;
    //     }

    //     memcpy(buf, fd + offset, size_to_rec);

    //     offset += size_to_rec;
    //     size_to_rec -= rec;
    // }




    int success;
    read_len(server_fd, &success, sizeof(int));

    if(success > 0)
    {
        // read_len(server_fd, buf, sizeof(char*));
        int size_to_rec = 0;
        while(size_to_rec < success)
        {
            int rec = recv(server_fd, buf + size_to_rec, success - size_to_rec, 0);

            if(rec <= 0)
            {
                if(rec != 0)
                    perror("Read failed");
                break;
            }

            size_to_rec = size_to_rec + rec;
        }
    }

    close(server_fd);

    // if(strcmp(path+1, "test_file") != 0) {
    //     // We only support one file (test_file)...
    //     return -ENOENT;
    // }

    // size_t len;
    // len = strlen(TEST_DATA);
    // if (offset < len) {
    //     if (offset + size > len) {
    //         size = len - offset;
    //     }
        /* Note how the read request may not start at the beginning of the file.
         * We take the offset into account here: */
    //     memcpy(buf, TEST_DATA + offset, size);
    // } else {
    //     size = 0;
    // }

    return size;
}

/* This struct maps file system operations to our custom functions defined
 * above. */
static struct fuse_operations netfs_client_ops = {
    .getattr = netfs_getattr,
    .readdir = netfs_readdir,
    .open = netfs_open,
    .read = netfs_read,
};

static void show_help(char *argv[]) {
    printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
    printf("File-system specific options:\n"
            "    --port=<n>          Port number to connect to\n"
            "                        (default: %d)"
            "\n", DEFAULT_PORT);
}

int main(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    /* Set up default options: */
    options.port = DEFAULT_PORT;

    /* Parse options */
    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1) {
        return 1;
    }

    if (options.show_help) {
        show_help(argv);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0] = (char*) "";
    }

    return fuse_main(args.argc, args.argv, &netfs_client_ops, NULL);
}
