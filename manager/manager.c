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
#include <unistd.h>
#include <pthread.h>
#include <stdint.h> 
#include "logging.h"

#include "mbroker/mbroker.h"

static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
}

void make_request(request* rqst, uint8_t code, char fifo_path_name[], char box_name[]){
    rqst->opcode = code;
    memcpy(rqst->client_pipe, fifo_path_name, sizeof(char) * PIPE_SIZE);
    memcpy(rqst->box_name, box_name, sizeof(char) * BOX_SIZE);
}

void make_request_without_box(request* rqst, uint8_t code, char fifo_path_name[]) {
    rqst->opcode = code;
    memcpy(rqst->client_pipe, fifo_path_name, sizeof(char) * PIPE_SIZE);
}

ssize_t send_request(int rp, request *rqst) {
    ssize_t ret = write(rp, rqst, sizeof(request));
    return ret;
}

int main(int argc, char **argv) {

    char *server_pipe_path;
    char client_fifo_path[PIPE_SIZE];
    char box_name[BOX_SIZE];

    memset(client_fifo_path, '\0', sizeof(char) * PIPE_SIZE);
    memset(box_name, '\0', sizeof(char) * BOX_SIZE);

    if (argc < 5 || argc > 6) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    // alloc data for the request 
    request *rqst = (request*)malloc(sizeof(request));

    // parse the command-line arguments regarding the pipes' names
    server_pipe_path = argv[2];
    strcpy(client_fifo_path, argv[3]);
    

    // opens server's pipe to send the request
    int server_pipe = open(server_pipe_path, O_WRONLY);

    // list boxes
    if (argc == 5) {
        make_request_without_box(rqst, BOX_LIST_REQUEST, client_fifo_path);
        send_request(server_pipe, rqst);
    }
    // create box
    else if (strcmp(argv[4], "create") == 0) {
        strcpy(box_name, argv[5]);
        make_request(rqst, BOX_CREATION_REQUEST, client_fifo_path, box_name);
        send_request(server_pipe, rqst);
    }
    // remove box
    else {
        strcpy(box_name, argv[5]);
        make_request(rqst, BOX_DELETION_REQUEST, client_fifo_path, box_name);
        send_request(server_pipe, rqst);
    } 

    // remove manager pipe if it does exist
    if (unlink(client_fifo_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[3],strerror(errno));
        exit(EXIT_FAILURE);
    }

    // create manager pipe
    if (mkfifo(client_fifo_path, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);        
    }

    // Server's answer
    dlt_crt_answer answ;

    int client_pipe = open(client_fifo_path, O_RDONLY);

    // Read the answer that came from the server
    ssize_t ret = read(client_pipe, &answ, sizeof(dlt_crt_answer));

    if (ret < 0) {
        printf("FAIL\n");
        exit(EXIT_FAILURE);
    }        

    // Handle the different types of answer
    if (answ.opcode == ANSWER_TO_BOX_CREATION || answ.opcode == ANSWER_TO_BOX_DELETION) {
        if (answ.return_code != 0)
            fprintf(stdout, "ERROR %s\n", answ.error_message);  
        else
            fprintf(stdout, "OK\n");
    }

    free(rqst);
    close(server_pipe);
    close(client_pipe);

    return 0;
}
