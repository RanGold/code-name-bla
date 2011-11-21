#define MAX_ROW_LENGTH 256
#define MAX_MAILS_PER_USER 32000
#define WELLCOME_MESSAGE "Welcome! I am simple-mail-server."

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

void initialliaze_users_array(int* usersAmount, User** users, FILE* usersFile) {

	int i;
	char curRow[MAX_ROW_LENGTH];
	char* temp;

	*usersAmount = count_rows(usersFile);
	*users = (User*) calloc(*usersAmount, sizeof(User));

	fseek(usersFile, 0, SEEK_SET);

	for (i = 0; i < *usersAmount; i++) {

		fgets(curRow, MAX_ROW_LENGTH, usersFile);

		temp = strtok(curRow, "\t");
		(*users)[i].name = (char*) calloc(strlen(temp) + 1, sizeof(char));
		strcpy((*users)[i].name, temp);

		temp = strtok(NULL, "\n");
		(*users)[i].password = (char*) calloc(strlen(temp) + 1, sizeof(char));
		strcpy((*users)[i].password, temp);

		(*users)[i].mails = calloc(MAX_MAILS_PER_USER, sizeof(Mail));
	}
}

int main(int argc, char** argv) {

	/* Variables declaration */
	short port = 6423;
	FILE* usersFile;
	int usersAmount, res;
	unsigned int len;
	User* users = NULL;
	int listenSocket, clientSocket;
	struct sockaddr_in serverAddr, clientAddr;
	Message message;

	/* Validate number of arguments */
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Error: Usage mail_server "
			"<users_file> [port]\n");
		return (-1);
	}

	if (argc == 3) {
		port = (short) atoi(argv[2]);
	}

	/* Get file for reading */
	usersFile = get_valid_file(argv[1]);
	if (usersFile == NULL) {
		return (-1);
	}

	fseek(usersFile, 0, SEEK_SET);

	initialliaze_users_array(&usersAmount, &users, usersFile);

	/* create listen socket - needs to be same as the client */
	listenSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (listenSocket == -1) {
		print_error();
		/* TODO: free stuff before */
		return -1;
	}

	/* prepare  address structure to bind listen socket */
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);

	/* bind listen socket - no other processes can bind to this port */
	res = bind(listenSocket, (struct sockaddr*) &serverAddr,
					sizeof(serverAddr));
	if (res == -1) {
		print_error();
		/* TODO: free stuff before */
		return -1;
	}

	/* start listening */
	res = listen(listenSocket, 1);
	if (res == -1) {
		print_error();
		/* TODO: free stuff before */
		return -1;
	}

	do {

		/* prepare structure for client(dest) address */
		len = sizeof(clientAddr);

		/* Start waiting till client connect */
		clientSocket = accept(listenSocket, (struct sockaddr*) &clientAddr, &len);
		if (clientSocket == -1) {
			print_error();
			/* TODO: free stuff before */
			return -1;
		}

		/* Preparing send data */
		res = prepare_message_from_string(WELLCOME_MESSAGE, &message);

		res = send_message(clientSocket, &message, &len);
	} while (1);

	/* TODO: a lot of inner frees and close sockets */
	free(users);
	return 0;
}
