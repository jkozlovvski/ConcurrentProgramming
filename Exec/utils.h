#ifndef MIM_UTILS_H
#define MIM_UTILS_H

#include <stdbool.h>
#include <stdio.h>

#define MAXLENGTH_COMMAND 512
#define MAP_ANONYMOUS 0x20
#define MAXLENGTH_OUTPUT 1024
#define MAX_N_TASKS 4096
#define MAX_PATH_LENGTH 1024

/*
 * Set or unset the 'close_on_exec' flag on a given descriptor.
 *
 * As the name suggests, this causes the descriptor to be automatically closed
 * at any exec*(). By default this flag is false, but it's a good idea to
 * basically always set it. Note that the flag is false for duplicated
 * descriptors after dup2().
 */
void set_close_on_exec(int file_descriptor, bool value);

/*
 * Close a pipe on exec.
 *
 * This is a convenience function that calls set_close_on_exec() on both ends of
 * the pipe.
 */
void close_pipe_on_exec(int pipe[2]);

/*
 * Split a string into space-delimited parts.
 *
 * The result is an array of null-terminated strings, ending with NULL.
 * It must be freed with free_split_string().
 *
 * The resulting parts don't contain any spaces.
 * The result has always as many parts as they are spaces, plus 1.
 * In particular, for an empty string, the result is {"", NULL}.
 * An initial or final space, or two consecutive spaces, result in an
 * empty-string part.
 */
char** split_string(const char* s);

void free_split_string(char** parts);
/*
 * Read a line from `file`.
 *
 * Read characters into buffer until a newline or EOF is encountered.
 * If we read anything (possibly just a newline), set buffer to a non-empty
 * null-terminated string and return true. Otherwise (immediate EOF), set buffer
 * to empty string and return false.
 *
 * The newline is included, if we end at one.
 * If more than `size_of_buffer - 1` characters would be read, exit(1).
 * If a null character is read, exit(1).
 * On any error, exit(1).
 *
 * size_of_buffer must be at least 2.
 */
bool read_line(char* buffer, size_t size_of_buffer, FILE* file);

/* Print (to stderr) information about all open descriptors in current process.
 */
void print_open_descriptors(void);

void check_barrier(int status);

#endif
