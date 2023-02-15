#include "commands.h"
#include "err.h"
#include "utils.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

task_t tasks[MAX_N_TASKS];
queue_t queue;

int main()
{

    char command[MAXLENGTH_COMMAND];
    char** parts = NULL;

    size_t current_task = 0;
    create_tasks(tasks, MAX_N_TASKS);
    create_queue(&queue);

    while (read_line(command, MAXLENGTH_COMMAND, stdin)) {
        parts = split_string(command);

        execute_command(parts, tasks, &current_task, &queue);
    }

    execute_command(NULL, tasks, &current_task, &queue);
}