/**
 * netfs_server.h
 *
 * NetFS file server implementation.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
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
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <pwd.h>
#include <semaphore.h>

#include "common.h"
#include "logging.h"
#include "net.h"

sem_t thread_semaphore;

/*
 * Handles each request from client 
 */
void handle_request(int client_fd) 
{
    struct netfs_msg_header req_header = { 0 };
    read_len(client_fd, &req_header, sizeof(struct netfs_msg_header));
    LOG("Handling request: [type %d; length %lld]\n",
        req_header.msg_type,
        req_header.msg_len);

    uint16_t type = req_header.msg_type;
    LOG("req_header type: %d\n", type);

    if(type == MSG_READDIR) 
    {
        LOG("%s\n", "MSG_READDIR");
        readdir_handler(client_fd, req_header);
        return;
    }
    else if(type == MSG_GETATTR)
    {
        LOG("%s\n", "MSG_GETATTR");
        getattr_handler(client_fd, req_header);
        return;
    }
    else if(type == MSG_OPEN)
    {
        LOG("%s\n", "MSG_OPEN");
        open_handler(client_fd, req_header);
        return;
    }
    else if(type == MSG_READ)
    {
        LOG("%s\n", "MSG_READ");
        read_handler(client_fd, req_header);
        return;
    }
    else 
    {
        LOG("%s\n", "error: Unknown request type\n"); 
        return; 
    }

    return;

}

/*
 * Scan through a directory and transmit them across the network to the client
 */
void readdir_handler(int client_fd, struct netfs_msg_header req_header) 
{
    char path[MAXIMUM_PATH] = { 0 };
    read_len(client_fd, path, req_header.msg_len);
    LOG("READDIR: %s\n", path);

    char full_path[MAXIMUM_PATH] = { 0 };
    strcpy(full_path, ".");
    strcat(full_path, path);
    //req: /
    //actual: ./

    DIR *directory;
    if ((directory = opendir(full_path)) == NULL) 
    {
        perror("opendir");
        close(client_fd);
        return;
    }

    // Write each directory entry
    uint16_t len;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) 
    {
        len = strlen(entry->d_name) + 1;
        write_len(client_fd, &len, sizeof(uint16_t));
        write_len(client_fd, entry->d_name, len);
    }

    // Last directory entry
    len = 0;
    write_len(client_fd, &len, sizeof(uint16_t));

    closedir(directory);
    close(client_fd); // Close socket connection
    return;
}

/*
 * Transmit the resulting struct directly over the network
 */
void getattr_handler(int client_fd, struct netfs_msg_header req_header) 
{
    struct passwd *pw_server;
    uid_t uid;

    uid = geteuid();
    pw_server = getpwuid(uid);

    char path[MAXIMUM_PATH] = { 0 };
    read_len(client_fd, path, req_header.msg_len);
    LOG("GETATTR: %s\n", path);

    // Return if at root directory
    if(strcmp(path, "/") == 0)
        return;

    char full_path[MAXIMUM_PATH] = { 0 };
    strcpy(full_path, ".");
    strcat(full_path, path);

    struct stat stbuf;
    struct attr_stat atst = {0};

    int stat_success = 0;
    if(stat(full_path, &stbuf) < 0)
    {
        LOG("%s\n", "Stat function failed");
        write_len(client_fd, &stat_success, sizeof(int));
        close(client_fd);
        return;
    }
    else
    {
        stat_success = 1;
        LOG("%s\n", "Stat function success");
        write_len(client_fd, &stat_success, sizeof(int));
    }

    // Add attributes to custom struct
    LOG("\n%s\n\n", "***IS FILE***");
    atst.ino = stbuf.st_ino;
    atst.uid = stbuf.st_uid; 
    atst.gid = stbuf.st_gid;
    atst.mode = stbuf.st_mode;
    atst.nlink = stbuf.st_nlink;
    atst.size = stbuf.st_size;
    atst.blocks = stbuf.st_blocks;
    atst.mtim = stbuf.st_mtim;

    // Check if server id matches file id
    if(pw_server->pw_uid == atst.uid)
        atst.uid = 1;
    else
        atst.uid = 0;

    // Change permissions to read-only
    printf("File information for %s before change\n", path);
    printf("---------------------------\n");
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

    int statchmod = atst.mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    printf("********chmod: %o\n", statchmod);

    // Change bits to read-only
    if(S_ISDIR(atst.mode))
        atst.mode = atst.mode & (S_IFDIR | 0555);
    else
        atst.mode = atst.mode & (S_IFREG | 0555);

    statchmod = atst.mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    printf("********chmod: %o\n\n", statchmod);

    printf("File information for %s after change\n", path);
    printf("---------------------------\n");
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

    // Write custom struct to client
    write_len(client_fd, &atst, sizeof(struct attr_stat));

    close(client_fd); // Close socket connection
    return;
}

/*
 * Opens file given and sends to file descriptor to client
 */
void open_handler(int client_fd, struct netfs_msg_header req_header)
{
    char path[MAXIMUM_PATH] = { 0 };
    read_len(client_fd, path, req_header.msg_len);
    LOG("OPEN: %s\n", path);

    char full_path[MAXIMUM_PATH] = { 0 };
    strcpy(full_path, ".");
    strcat(full_path, path);

    uint16_t success;

    /* Check here if read_handler problems occur */

    // Open file
    int fd = open(full_path, O_RDONLY);
    if(fd == -1)
    {
        success = -1;
        write_len(client_fd, &success, sizeof(uint16_t));
        close(client_fd);
        perror("open");
        return;
    }
    else
    {
        success = 0;
        write_len(client_fd, &success, sizeof(uint16_t));
    }

    close(client_fd);
    return;
}

/*
 * Receives file information and and send file data with sendfile()
 */
void read_handler(int client_fd, struct netfs_msg_header req_header)
{
    // int fd;
    size_t size;
    off_t offset;

    char path[MAXIMUM_PATH] = { 0 };
    read_len(client_fd, path, req_header.msg_len);
    LOG("READ: %s\n", path);

    char full_path[MAXIMUM_PATH] = { 0 };
    strcpy(full_path, ".");
    strcat(full_path, path);

    struct stat stbuf;
    struct attr_stat atst = {0};

    int stat_success = 0;
    if(stat(full_path, &stbuf) < 0)
    {
        LOG("%s\n", "Stat function failed");
        write_len(client_fd, &stat_success, sizeof(int));
        close(client_fd);
        return;
    }
    else
    {
        stat_success = 1;
        LOG("%s\n", "Stat function success");
        write_len(client_fd, &stat_success, sizeof(int));
    }

    int bytes_read = stbuf.st_size;

    // Open file
    int fd = open(full_path, O_RDONLY);

    // size
    read_len(client_fd, &size, sizeof(size_t));
    // offset
    read_len(client_fd, &offset, sizeof(off_t));

    // Create buffer with size of file
    // char *buf = malloc(sizeof(char) * size);
    // Returns the number of bytes read, zero indicates end of file
    // int bytes_read = pread(fd, buf, size, offset);
    // printf("bytes read: %d\n", bytes_read);

    write_len(client_fd, &bytes_read, sizeof(int));
    
    if(bytes_read > 0)
    {
        while(size > 0)
        {
            ssize_t sent = sendfile(client_fd, fd, &offset, size);
            printf("Sent: %d\n", sent);
            printf("Offset: %lld\n", offset);
            if(sent <= 0)
            {
                if(sent != 0)
                    perror("Read failed");

                break;
            }
            size -= sent;
        }
    }

    close(client_fd);
    return;
}

int main(int argc, char *argv[]) 
{
    // Change to directory provided
    chdir(argv[1]);
    // Set port 
    int port = DEFAULT_PORT;
    if(argc == 3)
        port = atoi(argv[2]);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(socket_fd, 10) == -1) {
        perror("listen");
        return 1;
    }

    LOG("Listening on port %d\n", port);

    // Limit the maximum number of processes to avoid overwhelming the system
    int num_processes = 0;
    int max_processes = 4;

    while(true) {

        if (num_processes < max_processes) {
            int wstat;
            int ret = waitpid(-1, &wstat, WNOHANG);
            if (ret > 0) {
                num_processes--;
            }
        } else {
            int wstat;
            wait(&wstat);
            num_processes--;
        }

        struct sockaddr_storage client_addr = { 0 };
        socklen_t slen = sizeof(client_addr);


        int client_fd = accept(
            socket_fd,
            (struct sockaddr *) &client_addr,
            &slen);

        if (client_fd == -1) {
            perror("accept");
            return 1;
        }

        char remote_host[INET_ADDRSTRLEN];
        inet_ntop(
                client_addr.ss_family,
                (void *) &(((struct sockaddr_in *) &client_addr)->sin_addr),
                remote_host,
                sizeof(remote_host));
        LOG("Accepted connection from %s\n", remote_host);

        pid_t pid = fork();
        num_processes++;
        if(pid == 0) 
        {
            handle_request(client_fd);
            close(client_fd);  
        }


        // close(client_fd);  

    }

    // close(socket_fd);

    return 0;

} 
