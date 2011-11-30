#include "common.h"

void print_error() {

	fprintf(stderr, "Error: %s\n", strerror(errno));
}

void print_error_message(char* message) {

	fprintf(stderr, "Error: %s\n", message);
}

int get_absolute_path(char* relPath, char** absPath) {

	wordexp_t exp_result;
	wordexp(relPath, &exp_result, 0);
	*absPath = calloc(strlen(exp_result.we_wordv[0]) + 1, 1);
	if (*absPath == NULL) {
		return (ERROR);
	}
	strncpy(*absPath, exp_result.we_wordv[0], strlen(exp_result.we_wordv[0]));
	wordfree(&exp_result);

	return (0);
}

/* Gets a file for reading */
FILE* get_valid_file(char* fileName, char* mode) {

	FILE* file;
	char* absPath;
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
		return (NULL);
	}

	/* Return file and free resources */
	free(absPath);
	return (file);
}
