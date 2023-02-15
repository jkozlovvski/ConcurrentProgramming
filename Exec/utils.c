#include "utils.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "err.h"

void set_close_on_exec(int file_descriptor, bool value)
{
    int flags = fcntl(file_descriptor, F_GETFD);
    ASSERT_SYS_OK(flags);
    if (value)
        flags |= FD_CLOEXEC;
    else
        flags &= ~FD_CLOEXEC;
    ASSERT_SYS_OK(fcntl(file_descriptor, F_SETFD, flags));
}

void close_pipe_on_exec(int pipe[2])
{
    set_close_on_exec(pipe[0], true);
    set_close_on_exec(pipe[1], true);
}

char** split_string(const char* s)
{
    size_t len = strlen(s);
    int spaces = 0;
    for (int i = 0; i < len; ++i)
        if (s[i] == ' ')
            spaces++;
    char** parts = calloc(spaces + 2, sizeof(char*));
    parts[spaces + 1] = NULL;
    int p = 0;
    int b = 0;
    for (int i = 0; i < len; ++i) {
        if (s[i] == ' ') {
            parts[p++] = strndup(s + b, i - b);
            b = i + 1;
        }
    }
    parts[p++] = strndup(s + b, len - b);
    assert(p == spaces + 1);
    return parts;
}

void free_split_string(char** parts)
{
    for (int i = 0; parts[i] != NULL; ++i)
        free(parts[i]);
    free(parts);
}

bool read_line(char* buffer, size_t size_of_buffer, FILE* file)
{
    if (size_of_buffer < 2)
        fatal("Buffer too small: %d\n", size_of_buffer);

    char* line = NULL;
    size_t n_bytes;
    ssize_t n_chars = getline(&line, &n_bytes, file);

    if (errno == EINVAL) {
        syserr("Getline failed with buffor.");
    }

    if (n_chars == -1) {
        if (ferror(file))
            syserr("Getline failed.");
        assert(feof(file));
        free(line);
        return false;
    }

    if (n_chars == 0) {
        free(line);
        assert(feof(file));
        free(line);
        return false;
    }

    size_t len = strlen(line);
    if (len < n_chars)
        fatal("Null character in input.");
    assert(n_chars == len);

    if (len + 1 > size_of_buffer)
        fatal("Line too long: %d > %d.", len, size_of_buffer - 1);
    memcpy(buffer, line, len + 1);
    buffer[strcspn(buffer, "\n")] = '\0';

    free(line);

    return true;
}

void print_open_descriptors(void)
{
    const char* path = "/proc/self/fd";

    // Iterate over all symlinks in `path`.
    // They represent open file descriptors of our process.
    DIR* dr = opendir(path);
    if (dr == NULL)
        fatal("Could not open dir: %s", path);

    struct dirent* entry;
    while ((entry = readdir(dr)) != NULL) {
        if (entry->d_type != DT_LNK)
            continue;

        // Make a c-string with the full path of the entry.
        char subpath[MAX_PATH_LENGTH];
        int ret = snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
        if (ret < 0 || ret >= (int)sizeof(subpath))
            fatal("Error in snprintf");

        // Read what the symlink points to.
        char symlink_target[MAX_PATH_LENGTH];
        ssize_t ret2 = readlink(subpath, symlink_target, sizeof(symlink_target) - 1);
        ASSERT_SYS_OK(ret2);
        symlink_target[ret2] = '\0';

        // Skip an additional open descriptor to `path` that we have until
        // closedir().
        if (strncmp(symlink_target, "/proc", 5) == 0)
            continue;

        fprintf(stderr, "Pid %d file descriptor %3s -> %s\n", getpid(),
            entry->d_name, symlink_target);
    }

    closedir(dr);
}

void check_barrier(int status)
{
    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD) {
        fprintf(stderr, "Error while waiting for barrier.");
        exit(EXIT_FAILURE);
    }
}
