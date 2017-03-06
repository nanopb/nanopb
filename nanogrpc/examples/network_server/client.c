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
#include <pb.h>

#include "fileproto.pb.h"
#include "fileproto.ng.h"
#include "ng.h"
#include "common.h"

/* DEFINE_FILL_WITH_ZEROS_FUNCTION(GrpcRequest_CS)
DEFINE_FILL_WITH_ZEROS_FUNCTION(GrpcResponse_CS) */

/* This is only for holding methods, etc. It has to be reimplemented
for client purposes. (temporary) */
ng_grpc_handle_t hGrpc;
Path Path_holder;
FileList FileList_holder;
ng_methodContext_t context;
/* pb_istream_t istream;
pb_ostream_t ostream; */

/**
 * Encodes request into stream. This function will be moved intofile
 * containing client functions.
 * @param  stream [description]
 * @param  field  [description]
 * @param  arg    [description]
 * @return        [description]
 */
bool encodeRequestCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
  ng_encodeMessageCallbackArgument_t *argument = (ng_encodeMessageCallbackArgument_t*)*arg;
  /* char *str = get_string_from_somewhere(); */
  if (!pb_encode_tag_for_field(stream, field))
      return false;
  /* we are encoding tag for bytes, but writeing submessage, */
  /* as long it is prepended with size same as bytes */
  /*return pb_encode_submessage(stream, method->response_fields, method->response_holder);*/
  return pb_encode_submessage(stream, argument->method->request_fields, argument->context->request);
}


/* This callback function will be called once for each filename received
 * from the server. The filenames will be printed out immediately, so that
 * no memory has to be allocated for them.
 */
bool printfile_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    FileInfo fileinfo = {};

    if (!pb_decode(stream, FileInfo_fields, &fileinfo))
        return false;

    printf("%-10lld %s\n", (long long)fileinfo.inode, fileinfo.name);

    return true;
}


void dummyCallback(void * a, void *b){

}

void myGrpcInit(){
  /* FileServer_service_init(); */
  /* ng_setMethodHandler(&SayHello_method, &Greeter_methodHandler);*/
  context.request = (void *)&Path_holder;
  context.response = &FileList_holder;
  ng_setMethodContext(&FileServer_ListFiles_method, &context);
  ng_setMethodCallback(&FileServer_ListFiles_method, (void *)&dummyCallback);
  /* ng_GrpcRegisterService(&hGrpc, &FileServer_service); */
  /* hGrpc.input = &istream;
  hGrpc.output = &ostream; */
}

/* This function sends a request to socket 'fd' to list the files in
 * directory given in 'path'. The results received from server will
 * be printed to stdout.
 */
bool listdir(int fd, char *path)
{
    GrpcRequest_CS gRequest = GrpcRequest_CS_init_zero;
    GrpcResponse_CS gResponse = GrpcResponse_CS_init_zero;
    bool validRequest;
    size_t requestSize;
    /* I will work here on pointer, because code will be moved later
    to library files */
    /*ng_method_t *method = &FileServer_ListFiles_method;*/
    /* Construct and send the request to server */
    {
        pb_ostream_t output = pb_ostream_from_socket(fd);
        uint8_t zero = 0;

        /* In our protocol, path is optional. If it is not given,
         * the server will list the root directory. */
        if (path == NULL)
        {
            Path_holder.has_path = false;
        }
        else
        {
            Path_holder.has_path = true;
            if (strlen(path) + 1 > sizeof(Path_holder.path))
            {
                fprintf(stderr, "Too long Path_holder.\n");
                return false;
            }

            strcpy(Path_holder.path, path);
        }


        gRequest.path_hash = 3;
        gRequest.path = "/FileServer/ListFiles";
        gRequest.data.funcs.encode = &encodeRequestCallback;
        ng_encodeMessageCallbackArgument_t arg;
        arg.method = &FileServer_ListFiles_method;
        arg.context = &context;
        gRequest.data.arg = &arg;

        validRequest = pb_get_encoded_size(&requestSize,
                                            GrpcRequest_CS_fields,
                                            &gRequest);

        if (!validRequest){
          fprintf(stderr, "Request not vlalid: %s\n", PB_GET_ERROR(&output));
          return false;
        }

        /* Encode the request. It is written to the socket immediately
         * through our custom stream. */
        if (!pb_encode(&output, GrpcRequest_CS_fields, &gRequest))
        {
            fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&output));
            return false;
        }

        /* We signal the end of request with a 0 tag. */
        pb_write(&output, &zero, 1);
    }

    /* Read back the response from server */
    {
        pb_istream_t istream = pb_istream_from_socket(fd);

        /* Give a pointer to our callback function, which will handle the
         * filenames as they arrive. */

        if (!pb_decode(&istream, GrpcResponse_CS_fields, &gResponse))
        {
            fprintf(stderr, "Decode failed: %s\n", PB_GET_ERROR(&istream));
            return false;
        }

        FileList_holder.file.funcs.decode = &printfile_callback;

        if (gResponse.data == NULL){
          fprintf(stderr, "no data\n");
          pb_release(GrpcResponse_CS_fields, &gResponse);
          return false;
        }
        pb_istream_t input = pb_istream_from_buffer(gResponse.data->bytes, gResponse.data->size);

        if (!pb_decode(&input, FileList_fields, &FileList_holder))
        {
            fprintf(stderr, "Decode response failed: %s\n", PB_GET_ERROR(&input));
            pb_release(GrpcResponse_CS_fields, &gResponse);
            return false;
        }
        pb_release(GrpcResponse_CS_fields, &gResponse);

        /* If the message from server decodes properly, but directory was
         * not found on server side, we get path_error == true. */
        if (FileList_holder.path_error)
        {
            fprintf(stderr, "Server reported error.\n");
            return false;
        }

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

    /* Connect to server running on localhost:1234 */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port = htons(1234);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("connect");
        return 1;
    }
    myGrpcInit();
    /* Send the directory listing request */
    if (!listdir(sockfd, path))
        return 2;

    /* Close connection */
    close(sockfd);

    return 0;
}
