#include "common.h"

void print_error() {

	fprintf(stderr, "Error: %s\n", strerror(errno));
}

void print_error_message(char* message) {

	fprintf(stderr, "Error: %s\n", message);
}

int handle_return_value(int res) {
	if (res == ERROR) {
		print_error();
	} else if (res == ERROR_LOGICAL) {
		print_error_message(INVALID_DATA_MESSAGE);
		res = ERROR;
	} else if (res == ERROR_INVALID_ID) {
		print_error_message(INVALID_ID_MESSAGE);
		res = ERROR_INVALID_ID;
	} else if (res == ERROR_SOCKET_CLOSED) {
		print_error_message(SOCKET_CLOSED_MESSAGE);
		res = ERROR;
	}

	return res;
}

int get_absolute_path(char* relPath, char** absPath) {

	wordexp_t expansionResult;
	int expLength, i;

	wordexp(relPath, &expansionResult, 0);
	if (expansionResult.we_wordc == 0) {
		return (ERROR);
	}
	expLength = 0;
	for (i = 0; i < expansionResult.we_wordc; i++) {
		expLength += strlen(expansionResult.we_wordv[i]);
	}
	*absPath = calloc(expLength + expansionResult.we_wordc, 1);
	if (*absPath == NULL) {
		return (ERROR);
	}

	strncpy(*absPath, expansionResult.we_wordv[0], strlen(expansionResult.we_wordv[0]));
	for (i = 1; i < expansionResult.we_wordc; i++) {
		strcat(*absPath, " ");
		strcat(*absPath, expansionResult.we_wordv[i]);
	}
	wordfree(&expansionResult);

	return (0);
}

FILE* get_valid_file(char* fileName, char* mode) {

	FILE* file;
	char* absPath = NULL;
	int res;

	/* Getting absolute path */
	res = get_absolute_path(fileName, &absPath);
	if (res == ERROR) {
		return (NULL);
	}

	/* Open, validate and return file */
	file = fopen(absPath, mode);
	if (file == NULL) {
		perror(absPath);
		free(absPath);
		return (NULL);
	}

	/* Return file and free resources */
	free(absPath);
	return (file);
}

void init_FD_sets(fd_set *readfds, fd_set *writefds, fd_set *errorfds) {
		if (readfds != NULL){
			FD_ZERO(readfds);
		}

		if (writefds != NULL){
			FD_ZERO(writefds);
		}

		if (errorfds != NULL){
			FD_ZERO(errorfds);
		}
	}
