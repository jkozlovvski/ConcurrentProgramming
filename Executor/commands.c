#include "commands.h"
#include "err.h"
#include "utils.h"
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// barrier used for command execution synchronization
task_t* last_task = NULL;
pthread_barrier_t barrier;

void create_tasks(task_t* tasks, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        tasks[i].task_number = i;
        ASSERT_ZERO(pthread_mutex_init(&tasks[i].stdout_mutex, NULL));
        ASSERT_ZERO(pthread_mutex_init(&tasks[i].stderr_mutex, NULL));
    }
}

void create_queue(queue_t* queue)
{
    queue->last_task = 0;
    queue->is_running = false;
    queue->how_many_tasks_dead = 0;
    ASSERT_ZERO(pthread_mutex_init(&queue->task_status, NULL));
    ASSERT_ZERO(pthread_mutex_init(&queue->command_status, NULL));
    ASSERT_ZERO(pthread_mutex_init(&queue->print_mutex, NULL));
    ASSERT_ZERO(pthread_mutex_init(&queue->queue_mutex, NULL));
}

void destroy_queue(queue_t* queue)
{
    ASSERT_ZERO(pthread_mutex_destroy(&queue->task_status));
    ASSERT_ZERO(pthread_mutex_destroy(&queue->command_status));
    ASSERT_ZERO(pthread_mutex_destroy(&queue->print_mutex));
    ASSERT_ZERO(pthread_mutex_destroy(&queue->queue_mutex));
}

void destroy_tasks(task_t* tasks)
{
    for (size_t i = 0; i < MAX_N_TASKS; i++) {
        ASSERT_ZERO(pthread_mutex_destroy(&tasks[i].stdout_mutex));
        ASSERT_ZERO(pthread_mutex_destroy(&tasks[i].stderr_mutex));
    }
}

run_info_t* create_run_info(task_t* task, char** parts, queue_t* queue)
{
    run_info_t* run_info = malloc(sizeof(run_info_t));
    run_info->task = task;
    run_info->parts = parts;
    run_info->queue = queue;
    return run_info;
}

out_info_t* create_output_info(task_t* task, int fd,
    stream_type_t stream_type)
{
    out_info_t* output_info = malloc(sizeof(out_info_t));
    output_info->task = task;
    output_info->fd = fd;
    output_info->stream_type = stream_type;
    return output_info;
}

command_t get_command(char** command)
{
    if (command == NULL) {
        return QUIT;
    }

    char* command_name = command[0];

    if (strcmp(command_name, "run") == 0) {
        return RUN;
    } else if (strcmp(command_name, "out") == 0) {
        return OUT;
    } else if (strcmp(command_name, "err") == 0) {
        return ERR;
    } else if (strcmp(command_name, "kill") == 0) {
        return KILL;
    } else if (strcmp(command_name, "sleep") == 0) {
        return SLEEP;
    } else if (strcmp(command_name, "quit") == 0) {
        return QUIT;
    }
    // we assume output is always correct
    return EMPTY;
}

void* manage_output(void* data)
{
    out_info_t* output_info = data;
    task_t* task = output_info->task;
    int fd = output_info->fd;
    char* saving_buffer;
    pthread_mutex_t* mutex;

    if (output_info->stream_type == STDOUT) {
        saving_buffer = task->stdout_buffer;
        mutex = &(task->stdout_mutex);
    } else {
        saving_buffer = task->stderr_buffer;
        mutex = &(task->stderr_mutex);
    }

    FILE* read_end = fdopen(fd, "r");
    char buffer[MAXLENGTH_OUTPUT];

    check_barrier(pthread_barrier_wait(&barrier));
    while (read_line(buffer, MAXLENGTH_OUTPUT, read_end)) {
        ASSERT_ZERO(pthread_mutex_lock(mutex));
        strncpy(saving_buffer, buffer, MAXLENGTH_OUTPUT);
        ASSERT_ZERO(pthread_mutex_unlock(mutex));
    }

    free(output_info);
    ASSERT_SYS_OK(fclose(read_end));
    return NULL;
}

void print_process_end(size_t number, int status)
{
    if (WIFEXITED(status))
        printf("Task %ld ended: status %d.\n", number, WEXITSTATUS(status));
    else
        printf("Task %ld ended: signalled.\n", number);
}

void* execute_run(void* data)
{
    run_info_t* info = data;
    task_t* task = info->task;
    char** parameters = info->parts;
    queue_t* queue = info->queue;

    int pipe_stdout[2];
    int pipe_stderr[2];

    ASSERT_SYS_OK(pipe(pipe_stdout));
    ASSERT_SYS_OK(pipe(pipe_stderr));

    // we run this so each started process closes its ends of pipes
    close_pipe_on_exec(pipe_stdout);
    close_pipe_on_exec(pipe_stderr);

    pid_t process_pid = fork();
    ASSERT_SYS_OK(process_pid);

    if (process_pid == 0) {

        ASSERT_SYS_OK(dup2(pipe_stdout[1], STDOUT_FILENO));
        ASSERT_SYS_OK(dup2(pipe_stderr[1], STDERR_FILENO));

        char** program_args = parameters + 1;

        ASSERT_SYS_OK(execvp(program_args[0], program_args));
        exit(1);
    }

    task->task_pid = process_pid;
    ASSERT_SYS_OK(close(pipe_stdout[1]));
    ASSERT_SYS_OK(close(pipe_stderr[1]));

    pthread_t stdout_thread;
    pthread_t stderr_thread;

    out_info_t* stdout_info = create_output_info(task, pipe_stdout[0], STDOUT);
    out_info_t* stderr_info = create_output_info(task, pipe_stderr[0], STDERR);

    ASSERT_ZERO(
        pthread_create(&stdout_thread, NULL, manage_output, (void*)stdout_info));
    ASSERT_ZERO(
        pthread_create(&stderr_thread, NULL, manage_output, (void*)stderr_info));

    printf("Task %ld started: pid %d.\n", task->task_number, process_pid);
    check_barrier(pthread_barrier_wait(&barrier));
    // we allow main process to run
    int status;
    ASSERT_SYS_OK(waitpid(process_pid, &status, 0));

    // we save infromation that task is dead
    ASSERT_ZERO(pthread_mutex_lock(&(queue->task_status)));
    queue->how_many_tasks_dead++;
    if (queue->how_many_tasks_dead == 1) {
        // first task want to take semaphore from main
        ASSERT_ZERO(pthread_mutex_lock(&(queue->command_status)));
    }
    ASSERT_ZERO(pthread_mutex_unlock(&(queue->task_status)));
    ASSERT_ZERO(pthread_join(stdout_thread, NULL));
    ASSERT_ZERO(pthread_join(stderr_thread, NULL));

    if (queue->is_running) {
        ASSERT_ZERO(pthread_mutex_lock(&(queue->queue_mutex)));
        queue->tasks[queue->last_task] = process_pid;
        queue->statuses[queue->last_task] = status;
        queue->task_number[queue->last_task] = task->task_number;
        queue->last_task = queue->last_task + 1;
        ASSERT_ZERO(pthread_mutex_unlock(&(queue->queue_mutex)));
    } else {
        ASSERT_ZERO(pthread_mutex_lock(&(queue->print_mutex)));
        if (last_task != NULL) {
            // we wait for last thread to finish printing
            // in queue fashion, this way there is only one extra thread in the end
            ASSERT_ZERO(pthread_join(last_task->run_thread, NULL));
            last_task->is_joined = true;
            last_task = task;
        }
        print_process_end(task->task_number, status);
        ASSERT_ZERO(pthread_mutex_unlock(&(queue->print_mutex)));
    }

    // we update information about dead task
    ASSERT_ZERO(pthread_mutex_lock(&(queue->task_status)));
    queue->how_many_tasks_dead--;
    if (queue->how_many_tasks_dead == 0) {
        // last task want to take semaphore from main
        ASSERT_ZERO(pthread_mutex_unlock(&(queue->command_status)));
    }
    ASSERT_ZERO(pthread_mutex_unlock(&(queue->task_status)));

    // task has finished, so we can free arguments and info pointer
    free_split_string(info->parts);
    free(info);
    return NULL;
}

void execute_sleep(size_t usec) { usleep(usec * 1000); }

void execute_out(task_t* task)
{
    ASSERT_ZERO(pthread_mutex_lock(&(task->stdout_mutex)));
    fprintf(stdout, "Task %ld stdout: '%s'.\n", task->task_number,
        task->stdout_buffer);
    ASSERT_ZERO(pthread_mutex_unlock(&(task->stdout_mutex)));
}

void execute_err(task_t* task)
{
    ASSERT_ZERO(pthread_mutex_lock(&(task->stderr_mutex)));
    fprintf(stdout, "Task %ld stderr: '%s'.\n", task->task_number,
        task->stderr_buffer);
    ASSERT_ZERO(pthread_mutex_unlock(&(task->stderr_mutex)));
}

void execute_kill(task_t* task)
{
    // we can ignore any errors
    kill(task->task_pid, SIGINT);
}

void execute_quit(task_t* tasks, size_t* current_task)
{
    for (size_t i = 0; i < *current_task; i++) {
        task_t* task = &tasks[i];
        if (waitpid(task->task_pid, NULL, WNOHANG) == 0) {
            // we enforce that task is killed, but can ignore the error
            kill(task->task_pid, SIGKILL);
        }
    }

    // now we wait for the thread handling process to end
    for (size_t i = 0; i < *current_task; i++) {
        task_t* task = &tasks[i];
        if (!task->is_joined)
            ASSERT_ZERO(pthread_join(task->run_thread, NULL));
    }
}

void join_last_thread()
{
    if (last_task != NULL) {
        ASSERT_ZERO(pthread_join(last_task->run_thread, NULL));
        last_task->is_joined = true;
        last_task = NULL;
    }
}

task_t* get_task(char** commands, task_t* tasks)
{
    size_t number = atoi(commands[1]);
    return &tasks[number];
}

size_t get_time(char** commands) { return atoi(commands[1]); }

void execute_command(char** parts, task_t* tasks, size_t* current_task,
    queue_t* queue)
{
    command_t command = get_command(parts);
    ASSERT_ZERO(pthread_barrier_init(&barrier, NULL, 4));

    ASSERT_ZERO(pthread_mutex_lock(&(queue->command_status)));
    queue->is_running = true;
    join_last_thread();
    ASSERT_ZERO(pthread_mutex_unlock(&(queue->command_status)));

    switch (command) {
    case RUN:
        ASSERT_ZERO(pthread_create(
            &(tasks[*current_task].run_thread), NULL, execute_run,
            (void*)create_run_info(&tasks[*current_task], parts, queue)));
        (*current_task)++;
        check_barrier(pthread_barrier_wait(&barrier));
        break;
    case OUT:
        execute_out(get_task(parts, tasks));
        break;
    case ERR:
        execute_err(get_task(parts, tasks));
        break;
    case KILL:
        execute_kill(get_task(parts, tasks));
        break;
    case SLEEP:
        execute_sleep(get_time(parts));
        break;
    case QUIT:
        execute_quit(tasks, current_task);
        break;
    case EMPTY:
        break;
    }

    ASSERT_ZERO(pthread_mutex_lock(&(queue->command_status)));

    ASSERT_ZERO(pthread_mutex_lock(&(queue->print_mutex)));
    size_t dead_processes = queue->last_task;
    for (int i = 0; i < dead_processes; i++)
        print_process_end(queue->task_number[i], queue->statuses[i]);
    queue->last_task = 0;
    queue->is_running = false;
    ASSERT_ZERO(pthread_mutex_unlock(&(queue->print_mutex)));

    ASSERT_ZERO(pthread_mutex_unlock(&(queue->command_status)));
    ASSERT_ZERO(pthread_barrier_destroy(&barrier));

    if (command != RUN && parts != NULL) {
        free_split_string(parts);
    }

    if (command == QUIT) {
        destroy_queue(queue);
        destroy_tasks(tasks);
        exit(0);
    }
}
