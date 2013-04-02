/* This is a simple TCP server that listens on port 1234 and provides lists
 * of files to clients, using a protocol defined in file_server.proto.
 *
 * It directly deserializes and serializes messages from network, minimizing
 * memory use.
 * 
 * For flexibility, this example is implemented using posix api.
 * In a real embedded system you would typically use some other kind of
 * a communication and filesystem layer.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "fileproto.pb.h"
#include "common.h"

bool listdir_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    DIR *dir = (DIR*) *arg;
    struct dirent *file;
    FileInfo fileinfo;
    
    while ((file = readdir(dir)) != NULL)
    {
        fileinfo.inode = file->d_ino;
        strncpy(fileinfo.name, file->d_name, sizeof(fileinfo.name));
        fileinfo.name[sizeof(fileinfo.name) - 1] = '\0';
        
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        
        if (!pb_encode_submessage(stream, FileInfo_fields, &fileinfo))
            return false;
    }
    
    return true;
}

void handle_connection(int connfd)
{
    ListFilesRequest request;
    ListFilesResponse response;
    pb_istream_t input = pb_istream_from_socket(connfd);
    pb_ostream_t output = pb_ostream_from_socket(connfd);
    DIR *directory;
    
    if (!pb_decode(&input, ListFilesRequest_fields, &request))
    {
        printf("Decode failed: %s\n", PB_GET_ERROR(&input));
        return;
    }
    
    directory = opendir(request.path);
    
    printf("Listing directory: %s\n", request.path);
    
    if (directory == NULL)
    {
        perror("opendir");
        
        response.has_path_error = true;
        response.path_error = true;
        response.file.funcs.encode = NULL;
    }
    else
    {
        response.has_path_error = false;
        response.file.funcs.encode = &listdir_callback;
        response.file.arg = directory;
    }
    
    if (!pb_encode(&output, ListFilesResponse_fields, &response))
    {
        printf("Encoding failed.\n");
    }
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    struct sockaddr_in servaddr;
    int reuse = 1;
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port = htons(1234);
    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind");
        return 1;
    }
    
    if (listen(listenfd, 5) != 0)
    {
        perror("listen");
        return 1;
    }
    
    for(;;)
    {
        connfd = accept(listenfd, NULL, NULL);
        
        if (connfd < 0)
        {
            perror("accept");
            return 1;
        }
        
        printf("Got connection.\n");
        
        handle_connection(connfd);
        
        printf("Closing connection.\n");
        
        close(connfd);
    }
}
