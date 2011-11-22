#define MAX_ROW_LENGTH 256
#define WELLCOME_MESSAGE "Welcome! I am simple-mail-server."
#define SERVER_USAGE_MSG "Error: Usage mail_server <users_file> [port]\n"

#include "common.h"

/* Gets a file for reading */
FILE* get_valid_file(char* fileName) {

	/* Open, validate and return file */
	FILE* file = fopen(fileName, "r");
	if (file == NULL) {
		perror(fileName);
		return (NULL);
	}

	/* Return file */
	return (file);
}

int count_rows(FILE* file) {

	int counter = 0;
	char curChar = 0, lastChar = 0;

	fseek(file, 0, SEEK_SET);

	while ((curChar = fgetc(file)) != EOF) {
		if (curChar == '\n') {
			counter++;
		}

		lastChar = curChar;
	}

	/* Some editors put \n before EOF */
	return (lastChar == '\n' ? counter : ++counter);
}

int initialliaze_users_array(int* usersAmount, User** users, char* filePath) {

	int i;
	FILE* usersFile;

	/* Get file for reading */
	usersFile = get_valid_file(filePath);
	if (usersFile == NULL) {
		return (-1);
	}

	*usersAmount = count_rows(usersFile);
	*users = (User*) calloc(*usersAmount, sizeof(User));

	fseek(usersFile, 0, SEEK_SET);

	for (i = 0; i < *usersAmount; i++) {

		if (fscanf(usersFile, "%s\t%s", (*users)[i].name, (*users)[i].password) != 2) {
			fclose(usersFile);
			return (-1);
		}

		(*users)[i].mailAmount = 0;
	}

	fclose(usersFile);
	return (0);
}

int main(int argc, char** argv) {

	/* Variables declaration */
	short port = 6423;
	int usersAmount, res;
	unsigned int len;
	User* users = NULL;
	int listenSocket, clientSocket;
	struct sockaddr_in serverAddr, clientAddr;
	Message message;

	/* Validate number of arguments */
	if (argc != 2 && argc != 3) {
		fprintf(stderr, SERVER_USAGE_MSG);
		return (-1);
	} else if (argc == 3) {
		port = (short) atoi(argv[2]);
	}

	res = initialliaze_users_array(&usersAmount, &users, argv[1]);
	if (res == -1) {
		print_error_message("Failed initiallizing users array");
	}

	/* Create listen socket - needs to be same as the client */
	listenSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (listenSocket == -1) {
		print_error();
		/* TODO: free stuff before */
		return (-1);
	}

	/* Prepare address structure to bind listen socket */
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);

	/* Bind listen socket - no other processes can bind to this port */
	res = bind(listenSocket, (struct sockaddr*) &serverAddr,
					sizeof(serverAddr));
	if (res == -1) {
		print_error();
		close(listenSocket);
		/* TODO: free stuff before */
		return -1;
	}

	/* Start listening */
	res = listen(listenSocket, 1);
	if (res == -1) {
		print_error();
		close(listenSocket);
		/* TODO: free stuff before */
		return -1;
	}

	do {

		/* Prepare structure for client(dest) address */
		len = sizeof(clientAddr);

		/* Start waiting till client connect */
		clientSocket = accept(listenSocket, (struct sockaddr*) &clientAddr, &len);
		if (clientSocket == -1) {
			print_error();
		} else {

			/* Preparing send data */
			res = prepare_message_from_string(WELLCOME_MESSAGE, &message);

			res = send_message(clientSocket, &message, &len);

			close(clientSocket);
		}
	} while (1);

	/* TODO: a lot of inner frees and close sockets */
	free(users);
	return 0;
}
