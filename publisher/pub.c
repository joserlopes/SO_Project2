#include "logging.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>

#include "mbroker/mbroker.h"
#include "fs/operations.h"

static void print_usage() {
    fprintf(stderr, "usage: pub <register_pipe_name> <pipe_name> <box_name>\n");
}

void send_msg(int tx, char const *str) {
    size_t len = strlen(str);
    size_t written = 0;
    ssize_t written_ssize_t = 0;

    while (written < len) {
        ssize_t ret = write(tx, str + written, len - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        written_ssize_t = (ssize_t)written;

        written_ssize_t += ret;
    }
} 

void make_request(request* rqst, uint8_t code, char fifo_path_name[], char box_name[]){
    rqst->opcode = code;
    memcpy(rqst->client_pipe, fifo_path_name, sizeof(char) * PIPE_SIZE);
    memcpy(rqst->box_name, box_name, sizeof(char) * BOX_SIZE);
}

ssize_t send_request(int rp, request *rqst) {
    ssize_t ret = write(rp, rqst, sizeof(request));
    return ret;
}

int main(int argc, char **argv) {

    char *server_pipe_path;
    char client_fifo_path[PIPE_SIZE];
    char box_name[BOX_SIZE];

    // Fill the client_fifo and the box_name with '\0'
    memset(client_fifo_path, '\0', sizeof(char) * PIPE_SIZE);
    memset(box_name, '\0', sizeof(char) * BOX_SIZE);

    if (argc < 5) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    // Alloc data for the request 
    request *rqst = (request*)malloc(sizeof(request));

    // Parse the command-line arguments
    server_pipe_path = argv[2];
    strcpy(client_fifo_path, argv[3]);
    strcpy(box_name, argv[4]);

    // Open server's pipe to send the request
    int server_pipe = open(server_pipe_path, O_WRONLY);
        if (server_pipe == -1) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

    // Saves request data in rqst
    make_request(rqst, PUB_REGISTER_REQUEST, client_fifo_path, box_name);

    // Sends the request through the server's pipe
    if (send_request(server_pipe, rqst) < 0) {
        close(server_pipe);
        printf("Queue is full");
        return 0;
    }


    // remove publisher pipe if it does exist
    if (unlink(client_fifo_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[3],strerror(errno));
        exit(EXIT_FAILURE);
    }

    // create publisher pipe
    if (mkfifo(client_fifo_path, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);        
    }


    int client_pipe = open(client_fifo_path, O_WRONLY);
        if (client_pipe == -1) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
    }  
    
    char message[MAX_MESSAGE_SIZE];
    int n = scanf("%s", message); 

    while(n == 1){
        if(write(client_pipe, message, sizeof(char) *  MAX_MESSAGE_SIZE) < 0){
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        n = scanf("%s", message);
    }

    close(client_pipe);

    // Close server's pipe for writing
    close(server_pipe);

    // Free request's memory
    free(rqst);

    return 0;
}
