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
#include "fs/operations.h"
#include "mbroker.h"
#include "fs/state.h"

#define FREE 1
#define TAKEN 0


static allocation_state_t *free_workers;
static worker_thread* workers;
unsigned int MAX_NUM_SESSIONS = 0;
char SERVER_PIPE_PATHNAME[PIPE_SIZE];
int SERVER_PIPE;
box_node *head;

void addBoxNode(box txt_box) {
    box_node *newNode = (box_node*) malloc(sizeof(box_node));
    newNode->txt_box = txt_box;
    newNode->next = head;
    head = newNode;
}

void removeNode(char box_name[]) {
    box_node *temp = head, *prev = NULL;
    while (temp != NULL) {
        if (strcmp(temp->txt_box.box_name, box_name) == 0){
            if (prev == NULL) {
                head = temp->next;
            } else {
                prev->next = temp->next;
            }
            free(temp);
            return;
    }
    prev = temp;
    temp = temp->next;
    }
}

void freeBoxList() {
    box_node* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
}

box_node* get_box(char box_name[]) {
    box_node* temp = head;
    while (temp != NULL) {
        if (strcmp(temp->txt_box.box_name, box_name) == 0){
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

ssize_t try2read(int fd, void *buf, size_t count) {
    ssize_t bytes_read;
    do {
        bytes_read = read(fd, buf, count);
    } while (bytes_read < 0 && errno == EINTR);
    return bytes_read;
}

ssize_t try2write(int fd, void *buf, size_t count) {
    ssize_t bytes_written;
    do {
        bytes_written = write(fd, buf, count);
    } while (bytes_written < 0 && errno == EINTR);
    return bytes_written;
}

int register_pub(worker_thread *thread) {
    box_node* txt_box = get_box(thread->prtcl.box_name);

    // if the box already has a publisher or if the box doesn't exist
    if (txt_box == NULL || txt_box->txt_box.pub != NULL){
        //enviar sinal para cancelar
        return 0;
    }

    strcpy(txt_box->txt_box.pub, thread->prtcl.client_pipe);
    thread->prtcl.opcode = PUB_MESSAGE;
    
    return 0;
}

int register_sub(worker_thread *thread) {
    box_node* node = get_box(thread->prtcl.box_name);
    if (node == NULL){
        return -1;
    }

    strcpy(node->txt_box.subs[node->txt_box.curr_n_subs], thread->prtcl.client_pipe);
    node->txt_box.curr_n_subs++;
    return 0;
}

int create_box(worker_thread *thread) {

    box *txt_box = malloc(sizeof(box));
    char buffer[BOX_SIZE] = "/";
    strcat(buffer, thread->prtcl.box_name);

    strcpy(txt_box->box_name,thread->prtcl.box_name);
    txt_box->curr_n_subs = 0;

    // create the box inside TFS
    int fd;
    if ((fd = tfs_open(buffer, TFS_O_CREAT)) == -1)
        return -1;
    // add the box the the list of boxes
    addBoxNode(*txt_box);

    thread->prtcl.opcode = ANSWER_TO_BOX_CREATION;

    return fd;
}

int delete_box(worker_thread *thread) {
    char buffer[BOX_SIZE] = "/";
    strcat(buffer, thread->prtcl.box_name);

    // removes the box from TFS
    if (tfs_unlink(buffer) < 0)
        return -1;

    removeNode(thread->prtcl.box_name);
    thread->prtcl.opcode = ANSWER_TO_BOX_DELETION;

    return 0;
}

int publish_message(worker_thread *thread) {
    printf("A");
    int fd, client_pipe;
    char buffer[BOX_SIZE] = "/";
    char message[MAX_MESSAGE_SIZE];
    strcat(buffer, thread->prtcl.box_name);

    client_pipe = open(thread->prtcl.client_pipe, O_RDONLY);

    if (client_pipe == -1) {
        return -1;
    }

    if (read(client_pipe, message, sizeof(char) * MAX_MESSAGE_SIZE) == -1)
        return -1;

    // open the box to write in the end
    if ((fd = tfs_open(buffer, TFS_O_APPEND)) == -1)
        return -1;
    
    // publish the message (write it on the file)
    if ((tfs_write(fd, message, sizeof(char) * MAX_MESSAGE_SIZE)) == -1)
        return -1;
    return fd;
}


int share_message(worker_thread *thread) {
    int fd, sub;
    char buffer[BOX_SIZE] = "/";
    char message[MAX_MESSAGE_SIZE];
    strcat(buffer, thread->prtcl.box_name);

    // open the box to read in the end FIXME ze qual e o sinal no tfs_open
    if ((fd = tfs_open(buffer, TFS_O_APPEND)) == -1)
        return -1;
    
    if((sub = open(thread->prtcl.client_pipe, O_WRONLY)) == -1){}
    while(true) {
        ssize_t ret = tfs_read(fd, message, sizeof(char) * MAX_MESSAGE_SIZE);
        if (ret == 0)
            break;
        else if(ret == -1)
            return -1;

        if(write(sub, message, sizeof(char) * MAX_MESSAGE_SIZE) == -1)
            return -1;
    }
    
    return fd;
}


int answer_box_dlt_crt(worker_thread *thread){

    int manager_pipe = open(thread->prtcl.client_pipe, O_WRONLY);

    dlt_crt_answer *answer = malloc(sizeof(dlt_crt_answer));

    answer->return_code = thread->prtcl.return_code;

    answer->opcode = thread->prtcl.opcode;

    strcpy(answer->error_message, thread->prtcl.error_message);

    ssize_t ret = write(manager_pipe, answer, sizeof(dlt_crt_answer));

    if(ret < 0)
        return -1;

    free(answer);
    return 0;
}

int box_list_request(worker_thread *thread){
    (void)thread;
    return 0;
}

void *session_handler(void *args) {
    worker_thread *thread = (worker_thread *)args;

    while (true) {
        pthread_mutex_lock(&thread->lock);

        while(thread->to_execute == false) {
            if (pthread_cond_wait(&thread->cond, &thread->lock) != 0) {
                perror("Failed to wait for conditional variable");
                exit(EXIT_FAILURE);
            }
        }

        int result = 0;
        switch (thread->prtcl.opcode) {
            case PUB_REGISTER_REQUEST:
                result = register_pub(thread);
                break;
            case SUB_REGISTER_REQUEST:
                result = register_sub(thread);
                break;
            case BOX_CREATION_REQUEST:
                result = create_box(thread);
                break;
            case ANSWER_TO_BOX_CREATION:
                result = answer_box_dlt_crt(thread);
                break;
            case BOX_DELETION_REQUEST:
                result = delete_box(thread);
                break;
            case ANSWER_TO_BOX_DELETION:
                result = answer_box_dlt_crt(thread);
                break;
            case BOX_LIST_REQUEST:
                result = box_list_request(thread);
                break;
            case ANSWER_TO_LISTING:
                break;
            case PUB_MESSAGE:
                result = publish_message(thread);
                break;
            case SUB_MESSAGE:
                result = share_message(thread);
                break;
            default:
                break;
        } 
        if (result < 0){
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&thread->lock);
    }
}

int init_mbroker() {
    // Allocate memory for the free_workers table 
    free_workers = malloc(MAX_NUM_SESSIONS * sizeof(allocation_state_t));

    // Allocate memory for the worker_tread table
    workers = (worker_thread *)malloc(MAX_NUM_SESSIONS * sizeof(worker_thread));

    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        workers[i].id = i;
        workers[i].to_execute = false;
        pthread_mutex_init(&workers[i].lock, NULL);
        pthread_cond_init(&workers[i].cond, NULL);
        if (pthread_create(&workers[i].threadID, NULL, session_handler, &workers[i]) != 0) {
            return -1;
        }

        free_workers[i] = FREE; 
    }
    return 0;
}

int destroy_mbroker() {
    free(free_workers);
    free(workers);
    freeBoxList();

    free_workers = NULL;
    workers = NULL;

    return 0;
}

int get_available_thread() {
    for (int i = 0; i < MAX_NUM_SESSIONS; ++i) {
        if (free_workers[i] == FREE) {
            free_workers[i] = TAKEN;
            return i;
        }
    }
    printf("All workers are full\n");
    return -1;
}

void general_parser(int parser_func(worker_thread *, request rqst), request rqst) {
    int tid = get_available_thread();
    if(tid < 0){
        return; //FULL
    }

    worker_thread *thread = &workers[tid];

    // Lock worker thread mutex
    pthread_mutex_lock(&thread->lock);
    thread->prtcl.opcode = rqst.opcode;

    int result = 0;
    if (parser_func != NULL){
        result = parser_func(thread, rqst);
    }

    // Worker thread can begin to execute
    if(result == 0) {
        thread->to_execute = true;   
        free_workers[tid] = TAKEN;
        if (pthread_cond_signal(&thread->cond) != 0) {
            perror("Couldn't signal worker");
            exit(EXIT_FAILURE);
        }
    } else {
        perror("Couldn't parse request");
        exit(EXIT_FAILURE);
    }

    // Unlock worker thread mutex
    pthread_mutex_unlock(&thread->lock);
}

int parser_basic_request(worker_thread *thread, request rqst) {
    strcpy(thread->prtcl.box_name, rqst.box_name);
    strcpy(thread->prtcl.client_pipe, rqst.client_pipe);
    return 0;
}

int parser_list_request(worker_thread *thread, request rqst) {
    thread->prtcl.opcode = rqst.opcode;
    strcpy(thread->prtcl.client_pipe, rqst.client_pipe);

    return 0;
}

/*int parser_list_answer(worker_thread *thread){
    list_answer answer;
    try2read(SERVER_PIPE, &answer, sizeof(list_answer));
    thread->prtcl.opcode = answer.opcode;
    thread->

}*/

/*int parser_dlt_crt_answer(worker_thread *thread) {
    dlt_crt_answer answer;
    try2read(SERVER_PIPE, &answer, sizeof(dlt_crt_answer));
    thread->prtcl.opcode = answer.opcode;
    thread->prtcl.return_code = answer.return_code;
    strcpy(thread->prtcl.error_message, answer.error_message);
    return 0;
}*/

int parser_message_request(worker_thread *thread, request rqst) {
    thread->prtcl.opcode = rqst.opcode;
    strcpy(thread->prtcl.message, rqst.message);

    return 0;
}

int main(int argc, char **argv) {

    tfs_params params = tfs_default_params();

    if (argc < 4) {
        fprintf(stderr, "usage: mbroker <pipename>\n");
        exit(EXIT_FAILURE);
    }

    // Turns the character from the arguments into the integer that it represents
    MAX_NUM_SESSIONS = (unsigned int)atoi(argv[3]);

   if (init_mbroker() != 0){
        printf("Failed to init server\n");
        exit(EXIT_FAILURE);
    }

    if (tfs_init(&params) != 0) {
        printf("Failed to init tfs\n");
        exit(EXIT_FAILURE);
    }

    strcpy(SERVER_PIPE_PATHNAME, argv[2]);

    // Remove server's pipe if it does not exist
    if (unlink(SERVER_PIPE_PATHNAME) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        exit(EXIT_FAILURE);
    }

    // Create the server's pipe
    if (mkfifo(SERVER_PIPE_PATHNAME, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);        
    }

    // Open server's pipe for receiving the requests
    SERVER_PIPE = open(SERVER_PIPE_PATHNAME, O_RDONLY);
    if (SERVER_PIPE == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    request rqst;

    // Request handler cycle
    while(true) {

        // ! Possivelmente por aqui uma dummy pipe
        
        bytes_read = try2read(SERVER_PIPE, &rqst, sizeof(request));
        
         while (bytes_read > 0) {

            switch(rqst.opcode) {
            case PUB_REGISTER_REQUEST:
                general_parser(parser_basic_request, rqst);
                break;
            case SUB_REGISTER_REQUEST:
                general_parser(parser_basic_request, rqst);
                break;
            case BOX_CREATION_REQUEST:
                general_parser(parser_basic_request, rqst);
                break;
            case BOX_DELETION_REQUEST:
                general_parser(parser_basic_request, rqst);
                break;
            case BOX_LIST_REQUEST:
                general_parser(parser_list_request, rqst);
                break;
            case PUB_MESSAGE:
                general_parser(parser_message_request, rqst);
                break;
            case SUB_MESSAGE:
                general_parser(parser_message_request, rqst);            
                break;
            default:
                break;
         }
            bytes_read = try2read(SERVER_PIPE, &rqst, sizeof(request));
        }

        if (bytes_read < 0) {
            printf("FAIL\n");
            exit(EXIT_FAILURE);
        }
    }

    close(SERVER_PIPE);
    unlink(SERVER_PIPE_PATHNAME);
    tfs_destroy();
    destroy_mbroker();
    return 0;
}


 
