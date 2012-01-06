/* System parameters */
#define MAX_NAME_LEN 50
#define MAX_PASSWORD_LEN 50

/* Errors */
#define ERROR -1
#define ERROR_SOCKET_CLOSED -2
#define ERROR_LOGICAL -3
#define ERROR_INVALID_ID -4

/* Errors Messages */
#define INVALID_DATA_MESSAGE "Invalid data received"
#define SOCKET_CLOSED_MESSAGE "Socket closed"
#define INVALID_ID_MESSAGE "Invalid id requested"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wordexp.h>

/* Print the current, indicated by errno, message */
void print_error();

/* Print a specific error message */
void print_error_message(char *message);

/* Handling different return values including errors */
int handle_return_value(int res);

/* Gets a file for whatever requested mode */
FILE* get_valid_file(char* fileName, char* mode);

/* Initializes the file descriptors given */
void init_FD_sets(fd_set *readfds, fd_set *writefds, fd_set *errorfds);
