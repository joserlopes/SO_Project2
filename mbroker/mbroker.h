#ifndef __MBROKER_H__
#define __MBROKER_H__

#define PIPE_SIZE (256)
#define BOX_SIZE (32)
#define MAX_MESSAGE_SIZE (1024)

/**
 * Multiple opcode
*/
#define PUB_REGISTER_REQUEST 1
#define SUB_REGISTER_REQUEST 2
#define BOX_CREATION_REQUEST 3
#define ANSWER_TO_BOX_CREATION 4
#define BOX_DELETION_REQUEST 5
#define ANSWER_TO_BOX_DELETION 6
#define BOX_LIST_REQUEST 7
#define ANSWER_TO_LISTING 8
#define PUB_MESSAGE 9
#define SUB_MESSAGE 10

/**
 * Serialization protocol
*/
typedef struct {
    uint8_t opcode; 
    char client_pipe[PIPE_SIZE];
    char box_name[BOX_SIZE];
    char message[MAX_MESSAGE_SIZE];
    int32_t return_code;
    char error_message[MAX_MESSAGE_SIZE];
    uint8_t last;
    uint64_t box_size;
    uint64_t n_publishers;
    uint64_t n_subscribers;
} protocol;

/**
 * Basic client request
*/
typedef struct {
    uint8_t opcode; 
    char client_pipe[PIPE_SIZE];
    char box_name[BOX_SIZE];
    char message[MAX_MESSAGE_SIZE];
} request;

/**
 * Answer to the box creation/deletion request
*/
typedef struct {
    uint8_t opcode;
    int32_t return_code;
    char error_message[MAX_MESSAGE_SIZE];
} dlt_crt_answer; 

/**
 * Answer to the box list request
*/
typedef struct {
    char box_name[BOX_SIZE];
    uint8_t last;
    uint64_t box_size;
    uint64_t n_publishers;
    uint64_t n_subscribers;
} list_answer;

/**
 * Simple struct of the worker_thread
*/
typedef struct {
    int id;
    pthread_t threadID;
    protocol prtcl;
    bool to_execute;
    pthread_mutex_t lock; 
    pthread_cond_t cond;
} worker_thread;

/**
 * Simple struct of a box
*/
typedef struct {
    char* subs[PIPE_SIZE];
    char pub[PIPE_SIZE];
    char box_name[BOX_SIZE];
    uint64_t curr_n_subs;
} box;

/**
 * Box node
*/
typedef struct box_node {
    box txt_box;
    struct box_node* next;
} box_node;

/**
 * Adds a new box_node to the beggining of the list of boxes
*/
void addBoxNode(box txt_box);

/**
 * Removes the box with 'box_name' from the list of boxes
*/
void removeNode(char box_name[]);

/**
 * Frees all the memory associated with the list of boxes
*/
void freeBoxList();

/**
 * Returns a pointer to the box_node with 'box_name' 
*/
box_node* get_box(char box_name[]);

/**
 * Loops through constant reads until something is read 
*/
ssize_t try2read(int fd, void *buf, size_t count);

/**
 * Loops through constant writes until something is written
*/
ssize_t try2write(int fd, void *buf, size_t count);

/**
 * Register the subscriber that is being handled by the current 
 * worker_thread in the server
*/
int register_pub(worker_thread *thread);

/**
 * Register the subscriber that is being handled by the current 
 * worker_thread in the server
*/
int register_sub(worker_thread *thread);

/**
 * Creates the box that is being handled by the current 
 * worker_thread in the server inside TFS
*/
int create_box(worker_thread *thread);

/**
 * Deletes the box that is being handled by the current 
 * worker_thread in the server from TFS
*/
int delete_box(worker_thread *thread);

/**
 * Publishes the message in the box that is being handled by the current 
 * worker_thread (writes the message to the file)
*/
int publish_message(worker_thread *thread);

/**
 * Main loop to handle the multiple worker_threads
*/
void *session_handler(void *args);

/**
 * Initialize mbroker
*/
int init_mbroker();

/**
 * Frees all memory associated with the mbroker
*/
int destroy_mbroker();

/**
 * Returns the index of the first avaliable thread
*/
int get_available_thread();

/**
 * General function to parse information received from the server's pipe
*/
void general_parser(int parser_func(worker_thread *, request rqst), request rqst);

/**
 * Function to parse a basic request from the client
*/
int parser_basic_request(worker_thread *thread, request rqst);

/**
 * Function to parse a listing request from the client
*/
int parser_list_request(worker_thread *thread, request rqst);

/**
 * Function to parse a creation/deletion request from the client
*/
int parser_dlt_crt_answer(worker_thread *thread, request rqst);

/**
 * Fucntion to parse the client's messages request
*/
int parser_message_request(worker_thread *thread, request rqst);

#endif