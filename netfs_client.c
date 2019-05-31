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
    char *server;
} options;

#define OPTION(t, p) { t, offsetof(struct options, p), 1 }

/* Command line option specification. We can add more here. If we're interested
 * in a string, specify --opt=%s .*/
static const struct fuse_opt option_spec[] = {
    OPTION("-h", show_help),
    OPTION("--help", show_help),
    OPTION("--port=%d", port),
    OPTION("--server=%s", server),
    FUSE_OPT_END
};

/*
 * NFS Get Attributes
 */
static int netfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    LOG("GETATTR: %s\n", path);

    struct passwd *pw_client;
    uid_t uid;

    uid = geteuid();
    pw_client = getpwuid(uid);

    struct netfs_msg_header req_header = { 0 };
    req_header.msg_type = MSG_GETATTR;
    req_header.msg_len =  strlen(path) + 1;

    int server_fd = connect_to(options.server, options.port);
    
    // We should check if server is less than 0 here...
    if(server_fd < 0)
    {
        perror("Socket failed");
        return 1;
    }

    LOG("server_fd: %d\n", server_fd);

    /* Clear the stat buffer */
    memset(stbuf, 0, sizeof(struct stat));

    /* By default, we will return 0 from this function (success) */
    int res = 0;

    struct attr_stat atst = { 0 };

    // Root Directory
    if(strcmp(path, "/") == 0) 
    {
        LOG("%s\n", "DIRECTORY");
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
    } 
    else if((path + 1) != NULL)
    {
        LOG("%s\n", "FILE");
        /* Incrementing the path pointer by 1 will remove the '/' from the start
         * of the path. We're comparing it with a hard-coded file name.
         *   - S_IFREG: indicates a regular file
         * We also hard-code the size of this file based on its contents: 'hello
         * world!' */

        write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));
        write_len(server_fd, path, req_header.msg_len);

        int stat_success = 0;
        read_len(server_fd, &stat_success, sizeof(int));
        if(stat_success == 0)
        {
            LOG("%s\n", "Stat function couldn't read file");
            return -ENOENT;
        }

        read_len(server_fd, &atst, sizeof(struct attr_stat));

        // Check if client id mathces file id
        if(atst.uid == 1)
            atst.uid = pw_client->pw_uid;

        // Add appropriate attributes
        stbuf->st_ino = atst.ino;
        stbuf->st_uid = atst.uid;
        stbuf->st_gid = atst.gid;
        stbuf->st_mode = atst.mode;
        stbuf->st_nlink = atst.nlink;
        stbuf->st_size = atst.size;
        stbuf->st_blocks = atst.blocks;
        stbuf->st_mtim = atst.mtim;
    }
    else
    {
        /* -ENOENT = 'no such file or directory'  */
        res = -ENOENT;
    }

    close(server_fd);
    
    return res;

}

/*
 * Read contents of directory given by server
 */ 
static int netfs_readdir(
        const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
        struct fuse_file_info *fi, enum fuse_readdir_flags flags) 
{

    LOG("READDIR: %s\n", path);

    /* By default, we will return 0 from this function (success) */
    int res = 0;

    struct netfs_msg_header req_header = { 0 };
    req_header.msg_type = MSG_READDIR;
    req_header.msg_len =  strlen(path) + 1;

    LOG("msg_readdir: %d\n", MSG_READDIR);
    LOG("path: %s\n", path);

    int server_fd = connect_to(options.server, options.port);
    
    // We should check if server is less than 0 here...
    if(server_fd < 0)
    {
        perror("Socket failed");
        return 1;
    }

    LOG("server_fd: %d\n", server_fd);

    write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));
    write_len(server_fd, path, req_header.msg_len);

    uint16_t reply_len = 1;
    char reply_path[MAXIMUM_PATH] = { 0 };

    // Keep taking in files that server sends
    while(reply_len > 0)
    {
        read_len(server_fd, &reply_len, sizeof(uint16_t));
        read_len(server_fd, reply_path, reply_len);

        // Break to end loop and no duplicate file/folder appears
        if(reply_len == 0)
            break;

        printf("-> %s\n", reply_path);
        printf("reply_len: %d\n", reply_len);

        filler(buf, reply_path, NULL, 0, 0); 
    }

    close(server_fd);

    return res;
}

/*
 * Takes in integer from open to check if file opened correctly. If so then,
 * save file descriptor in to fuse_file_info file handler (fh) for read
 */
static int netfs_open(const char *path, struct fuse_file_info *fi) 
{

    LOG("OPEN: %s\n", path);

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

    int server_fd = connect_to(options.server, options.port);
    
    // We should check if server is less than 0 here...
    if(server_fd < 0)
    {
        perror("Socket failed");
        return 1;
    }

    LOG("server_fd: %d\n", server_fd);

    write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));
    write_len(server_fd, path, req_header.msg_len);

    uint16_t success;

    // Read in value whether open was successful or not
    read_len(server_fd, &success, sizeof(uint16_t));

    if(success == -1)
    {
        res = 1;
        LOG("%s\n", "Open Failure");
    }
    else
    {
        LOG("%s\n", "Open Successful");
    }

    close(server_fd);
    
    return res;
}

/* 
 * Sends file information to server and takes in bytes from server to store to buffer
 */
static int netfs_read(
        const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) 
{

    LOG("READ: %s\n", path);

    struct netfs_msg_header req_header = { 0 };
    req_header.msg_type = MSG_READ;
    req_header.msg_len =  strlen(path) + 1;

    int server_fd = connect_to(options.server, options.port);
    
    // We should check if server is less than 0 here...
    if(server_fd < 0)
    {
        perror("Socket failed");
        return 1;
    }

    LOG("server_fd: %d\n", server_fd);

    write_len(server_fd, &req_header, sizeof(struct netfs_msg_header));
    write_len(server_fd, path, req_header.msg_len);

    int stat_success = 0;
    read_len(server_fd, &stat_success, sizeof(int));
    if(stat_success == 0)
    {
        LOG("%s\n", "Stat function couldn't read file");
        return -ENOENT;
    }

    // File Size
    write_len(server_fd, &size, sizeof(size_t));
    // Offset
    write_len(server_fd, &offset, sizeof(off_t));

    // Get number of bytes to read
    int bytes_read;
    read_len(server_fd, &bytes_read, sizeof(int));
    // If read was successful in server side
    if(bytes_read > 0)
    {
        int size_to_rec = 0;
        while(size_to_rec < bytes_read)
        {
            int rec = recv(server_fd, buf + size_to_rec, bytes_read - size_to_rec, 0);
            printf("Rec: %d\n", rec);

            if(rec <= 0)
            {
                if(rec != 0)
                    perror("Read failed");

                break;
            }

            size_to_rec += rec;
        }

    }

    close(server_fd);

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
    options.server = NULL;

    /* Parse options */
    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1) {
        return 1;
    }

    if(options.server == NULL)
    {
        LOG("%s\n", "Server is NULL");
        return 1;
    }

    if (options.show_help) {
        show_help(argv);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0] = (char*) "";
    }

    return fuse_main(args.argc, args.argv, &netfs_client_ops, NULL);
}
