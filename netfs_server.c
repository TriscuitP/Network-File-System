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

#include "common.h"
#include "logging.h"
#include "net.h"

char* home_path;

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
    }

}

void readdir_handler(int client_fd, struct netfs_msg_header req_header) 
{
    char path[MAXIMUM_PATH] = { 0 };
    read_len(client_fd, path, req_header.msg_len);
    LOG("readdir: %s\n", path);

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

    uint16_t len;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) 
    {
        len = strlen(entry->d_name) + 1;
        write_len(client_fd, &len, sizeof(uint16_t));
        write_len(client_fd, entry->d_name, len);
        // printf("-> %s\n", entry->d_name);
    }

    // Last directory entry
    len = 0;
    write_len(client_fd, &len, sizeof(uint16_t));

    closedir(directory);
    close(client_fd); // Close socket connection

    return;
}

void getattr_handler(int client_fd, struct netfs_msg_header req_header) 
{
    char path[MAXIMUM_PATH] = { 0 };
    read_len(client_fd, path, req_header.msg_len);
    LOG("getattr: %s\n", path);

    // Return if at root directory
    if(strcmp(path, "/") == 0)
        return;

    char full_path[MAXIMUM_PATH] = { 0 };
    strcpy(full_path, ".");
    strcat(full_path, path);

    struct stat stbuf;
    struct attr_stat atst = {0};


    if(stat(full_path, &stbuf) < 0)
    {
        close(client_fd);
        return;
    }

    printf("\n***IS FILE***\n\n");
    atst.ino = stbuf.st_ino;
    atst.uid = stbuf.st_uid;
    atst.gid = stbuf.st_gid;
    atst.mode = stbuf.st_mode;
    atst.nlink = stbuf.st_nlink;
    atst.size = stbuf.st_size;
    atst.blocks = stbuf.st_blocks;
    atst.mtim = stbuf.st_mtim;
    write_len(client_fd, &atst, sizeof(struct attr_stat));

    close(client_fd); // Close socket connection

    // struct dirent *entry;

    // while((entry = readdir(directory)) != NULL) 
    // {
        // sprintf(path,"%s/%s", home_path, entry->d_name);
        // printf("Path: %s\n", path);
    //     printf("HERE\n");
    //     stat(entry->d_name, &stbuf);
    //     printf("HERE2\n");
    //     atst->ino = stbuf.st_ino;
    //     printf("HERE3\n");
    //     atst->uid = stbuf.st_uid;
    //     atst->gid = stbuf.st_gid;
    //     atst->mode = stbuf.st_mode;
    //     atst->nlink = stbuf.st_nlink;
    //     atst->size = stbuf.st_size;
    //     write_len(client_fd, atst, sizeof(struct attr_stat));
    // }

    // Last directory entry
    // write_len(client_fd, &len, sizeof(uint16_t));

    // closedir(directory);
    // close(client_fd); // Close socket connection
    

    
    // printf("HERE3\n");
    // printf( (S_ISDIR(atst->mode)) ? "d" : "-");

    // if(S_ISREG(stbuf->st_mode))
    // {
        // printf("IS FILE\n");
        // atst->ino = stbuf->st_ino;
        // atst->uid = stbuf->st_uid;
        // atst->gid = stbuf->st_gid;
        // atst->mode = stbuf->st_mode;
        // atst->nlink = stbuf->st_nlink;
        // atst->size = stbuf->st_size;
        // write_len(client_fd, atst, sizeof(struct attr_stat));
    //     close(client_fd);
    // }
    // else if(S_ISDIR(stbuf->st_mode))
    // {
    //     printf("IS DIRC\n");
    //     write_len(client_fd, atst, sizeof(struct attr_stat));
    //     close(client_fd);
    // }

    // if(stat(full_path, stbuf) < 0) 
    // {
    //     printf("***Path stat function failed***\n");
        // perror("Perror ");
        // close(client_fd);
        // return;
        
    // }

    // if(atst != NULL)
    // atst = NULL;
    // write_len(client_fd, atst, sizeof(struct attr_stat));
    // else
    // printf("STAT FUNCTION SUCCESS\n");

    
    return;
}

void open_handler(int client_fd, struct netfs_msg_header req_header)
{
    char path[MAXIMUM_PATH] = { 0 };
    read_len(client_fd, path, req_header.msg_len);
    LOG("open: %s\n", path);

    char full_path[MAXIMUM_PATH] = { 0 };
    strcpy(full_path, ".");
    strcat(full_path, path);

    uint16_t success;

    /* Check here if read_handler problems occur */
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

    // Send file descriptor for read
    write_len(client_fd, &fd, sizeof(int));

    close(client_fd);
    return;
}

void read_handler(int client_fd, struct netfs_msg_header req_header)
{
    int fd;
    size_t size;
    off_t offset;

    // fd
    read_len(client_fd, &fd, sizeof(int));
    // size
    read_len(client_fd, &size, sizeof(size_t));
    // offset
    read_len(client_fd, &offset, sizeof(off_t));


    // for(size_t size_to_send = size; size_to_send > 0; )
    // {
    //     ssize_t sent = sendfile(client_fd, fd, &offset, size_to_send);
    //     if(sent <= 0)
    //     {
    //         if(sent != 0)
    //             perror("sendfile");
    //         break;
    //     }

    //     offset += sent;
    //     size_to_send -= sent;
    // }


    char *buf = (char*)malloc(sizeof(char) * size);
    int success = pread(fd, buf, size, offset);

    write_len(client_fd, &success, sizeof(int));
    
    if(success >= 0)
    {
        // write_len(client_fd, buf, sizeof(char*));
        for(size_t size_to_send = size; size_to_send > 0; )
        {
            ssize_t sent = sendfile(client_fd, fd, &offset, size_to_send);
            if (sent <= 0)
            {
                if (sent != 0)
                    perror("sendfile");
                break;
            }

            offset += sent;
            size_to_send -= sent;
        }
    }

    close(client_fd);

    return;
}

int main(int argc, char *argv[]) 
{

    /* This starter code will initialize the server port and wait to receive a
     * message. */

    // chdir to the directory youre going to serve 
    /* TODO: Check start of path if it is home */
    chdir(argv[1]);
    home_path = argv[1];

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DEFAULT_PORT);
    if (bind(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(socket_fd, 10) == -1) {
        perror("listen");
        return 1;
    }

    LOG("Listening on port %d\n", DEFAULT_PORT);

    while(true) {
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

        handle_request(client_fd);
        // TODO: going to have to fork for other parts of P3
    }

    return 0;

} 
