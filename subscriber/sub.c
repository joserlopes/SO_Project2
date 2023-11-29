#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <unistd.h>

#include "mbroker/mbroker.h"


#define BUFFER_SIZE (128)

static void print_usage() {
    fprintf(stderr, "usage: sub <register_pipe_name> <pipe_name> <box_name>\n");
}

void receive_message(int rx){
    
    char buffer[BUFFER_SIZE];
    while (true) {
            ssize_t ret = read(rx, buffer, BUFFER_SIZE - 1);
            if (ret == 0) {
                // ret == 0 indicates EOF
                fprintf(stderr, "[INFO]: pipe closed\n");
                close(rx);
            } else if (ret == -1) {
                // ret == -1 indicates error
                fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }

            buffer[ret] = 0;
            // fprintf(stdout, buffer);
        }
}

void make_request(request * rqst, u_int8_t code, char pipe_path[], char box_name[]) {
    rqst->opcode = code;
    memcpy(rqst->client_pipe, pipe_path, sizeof(char) * PIPE_SIZE);
    memcpy(rqst->box_name, box_name, sizeof(char) * BOX_SIZE);
}

ssize_t send_request(int rp, request *rqst) {
    ssize_t ret = read(rp, rqst, sizeof(request));
    return ret;
}

int main(int argc, char** argv) {

    char *server_pipe_path;
    char client_pipe_path[PIPE_SIZE];
    char box_name[BOX_SIZE];

    // fill the client_fifo and the box_name with '\0'
    memset(client_pipe_path, '\0', sizeof(char) * PIPE_SIZE);
    memset(box_name, '\0', sizeof(char) * BOX_SIZE);

    if (argc < 5) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    // alloc data for the request 
    request *rqst = (request*)malloc(sizeof(request));

    // parse the command-line arguments
    server_pipe_path = argv[2];
    strcpy(client_pipe_path, argv[3]);
    strcpy(box_name, argv[4]);

    // open server's pipe to send the request
    int server_pipe_2write = open(server_pipe_path, O_WRONLY);
    if (server_pipe_2write == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // saves request data in rqst 
    make_request(rqst, SUB_REGISTER_REQUEST, server_pipe_path, box_name);

    // sends the request through the server's pipe
    if(send_request(server_pipe_2write, rqst) < 0){
        close(server_pipe_2write);
        free(rqst);
        printf("Queue is full");
        return 0;
    }

    // remove publisher pipe if it does exist
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[3],strerror(errno));
        exit(EXIT_FAILURE);
    }

    // create publisher pipe
    if (mkfifo(client_pipe_path, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);        
    }


    int client_pipe = open(client_pipe_path, O_WRONLY);
        if (client_pipe == -1) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
    }  

    char message[MAX_MESSAGE_SIZE];
    int n = 1;
    while(n == 1){
        if(read(client_pipe, message, sizeof(char) *  MAX_MESSAGE_SIZE) < 0){
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        n = printf("%s", message);
    }

    close(client_pipe);

    // close server's pipe for writing
    close(server_pipe_2write);

    // free request's memory 
    free(rqst);

    return 0;
}
