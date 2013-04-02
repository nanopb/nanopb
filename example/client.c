/* This is a simple TCP client that connects to port 1234 and prints a list
 * of files in a given directory.
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

bool printfile_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    FileInfo fileinfo;
    
    if (!pb_decode(stream, FileInfo_fields, &fileinfo))
        return false;
    
    printf("%-10lld %s\n", (long long)fileinfo.inode, fileinfo.name);
    
    return true;
}

bool listdir(int fd, char *path)
{
    ListFilesRequest request;
    ListFilesResponse response;
    pb_istream_t input = pb_istream_from_socket(fd);
    pb_ostream_t output = pb_ostream_from_socket(fd);
    uint8_t zero = 0;
    
    if (path == NULL)
    {
        request.has_path = false;
    }
    else
    {
        request.has_path = true;
        if (strlen(path) + 1 > sizeof(request.path))
        {
            fprintf(stderr, "Too long path.\n");
            return false;
        }
        
        strcpy(request.path, path);
    }
    
    if (!pb_encode(&output, ListFilesRequest_fields, &request))
    {
        fprintf(stderr, "Encoding failed.\n");
        return false;
    }
    
    /* We signal the end of request with a 0 tag. */
    pb_write(&output, &zero, 1);
    
    response.file.funcs.decode = &printfile_callback;
    
    if (!pb_decode(&input, ListFilesResponse_fields, &response))
    {
        fprintf(stderr, "Decode failed: %s\n", PB_GET_ERROR(&input));
        return false;
    }
    
    if (response.path_error)
    {
        fprintf(stderr, "Server reported error.\n");
        return false;
    }
    
    return true;
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr;
    char *path = NULL;
    
    if (argc > 1)
        path = argv[1];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port = htons(1234);
    
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("connect");
        return 1;
    }
    
    if (!listdir(sockfd, path))
        return 2;
    
    close(sockfd);
    
    return 0;
}
