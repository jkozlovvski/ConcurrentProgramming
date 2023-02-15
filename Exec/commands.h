#include "utils.h"
#include <pthread.h>
#include <semaphore.h>

typedef enum command { RUN,
    OUT,
    ERR,
    KILL,
    SLEEP,
    QUIT,
    EMPTY } command_t;

typedef struct queue {
    pid_t tasks[MAX_N_TASKS];
    size_t task_number[MAX_N_TASKS];
    int statuses[MAX_N_TASKS];

    size_t last_task;
    size_t how_many_tasks_dead;
    bool is_running;
    pthread_mutex_t task_status;
    pthread_mutex_t command_status;
    pthread_mutex_t print_mutex;
    pthread_mutex_t queue_mutex;
} queue_t;

typedef struct task {
    char stdout_buffer[MAXLENGTH_OUTPUT];
    char stderr_buffer[MAXLENGTH_OUTPUT];
    pthread_t run_thread;
    pthread_mutex_t stdout_mutex;
    pthread_mutex_t stderr_mutex;
    pid_t task_pid;
    size_t task_number;
    bool is_joined;
} task_t;

typedef struct run_info {
    task_t* task;
    queue_t* queue;
    char** parts;
} run_info_t;

typedef enum stream_type { STDOUT,
    STDERR } stream_type_t;

typedef struct out_info {
    task_t* task;
    int fd;
    stream_type_t stream_type;
} out_info_t;

void create_tasks(task_t* tasks, size_t n);

void create_queue(queue_t* queue);

void execute_command(char** parts, task_t* tasks, size_t* current_task,
    queue_t* queue);

void destroy_tasks(task_t* tasks);

void destroy_queue(queue_t* queue);
