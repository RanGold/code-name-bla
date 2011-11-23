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

void free_users_array(User *users, int usersAmount) {

	int i, j, k;

	for (i = 0; i < usersAmount; i++) {
		for (j = 0; j < users[i].mailAmount; j++) {
			for (k = 0; k < users[i].mails[j].numAttachments; j++) {
				free(users[i].mails[j].attachments[k].data);
				free(users[i].mails[j].attachments[k].fileName);
			}
			free(users[i].mails[j].attachments);
			free(users[i].mails[j].body);
			free(users[i].mails[j].sender);
			free(users[i].mails[j].subject);
		}
		free(users[i].mails);
	}

	free(users);
}

User* check_credentials_message(User* users, int usersAmount, Message *message) {

	char userName[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	int i;

	if (message->messageType != Credentials) {
		return (NULL);
	}

	if (sscanf((char*)message->data, "%s\t%s", userName, password) != 2) {
		return (NULL);
	}

	for (i = 0; i < usersAmount; i++) {
		if ((strcmp(users[i].name, userName) == 0) && (strcmp(users[i].password, password) == 0)) {
			return (users + i);
		}
	}

	return (NULL);
}

int prepare_message_from_inbox_content(User *user, Message* message) {

	int i, messageSize = 1;
	char temp[100];
	char *messageText;

	message->messageType = InboxContent;

	/* Calculating message size */
	for (i = 0; i < user->mailAmount; i++) {
		/* Adding white spaces and apostrophes */
		messageSize += 6;
		sprintf(temp, "%d", user->mails[i].id);
		messageSize += strlen(temp);
		messageSize += strlen(user->mails[i].sender);
		messageSize += strlen(user->mails[i].subject);
		sprintf(temp, "%d", user->mails[i].numAttachments);
		messageSize += strlen(temp);
	}

	messageText = (char*)calloc(messageSize, 1);
	if (messageText == NULL) {
		return (-1);
	}

	for (i = 0; i < user->mailAmount; i++) {
		sprintf(messageText, "%d %s \"%s\" %d\n",
				user->mails[i].id,
				user->mails[i].sender,
				user->mails[i].subject,
				user->mails[i].numAttachments);
	}

	message->dataSize = messageSize;
	message->data = (unsigned char*)messageText;

	return (0);
}

int main(int argc, char** argv) {

	/* Variables declaration */
	short port = 6423;
	int usersAmount, res;
	unsigned int len;
	User *users = NULL, *curUser = NULL;
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
		free_users_array(users, usersAmount);
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
		free_users_array(users, usersAmount);
		return -1;
	}

	/* Start listening */
	res = listen(listenSocket, 1);
	if (res == -1) {
		print_error();
		close(listenSocket);
		free_users_array(users, usersAmount);
		return (-1);
	}

	message.data = NULL;

	do {

		/* Prepare structure for client address */
		len = sizeof(clientAddr);

		/* Start waiting till client connect */
		clientSocket = accept(listenSocket, (struct sockaddr*) &clientAddr, &len);
		if (clientSocket == -1) {
			print_error();
		} else {

			if ((prepare_message_from_string(WELLCOME_MESSAGE, &message) == -1)
					|| (send_message(clientSocket, &message, &len) == -1)) {
				print_error();
			} else {
				/* TODO : decide how to handle when message send fails, maybe reset? */
				do {
					res = recv_message(clientSocket, &message, &len);
					if (res != 0) {
						if (res == -1) {
							print_error();
						}
						break;
					}

					if (message.messageType == Quit) {
						break;
					} else if (curUser == NULL) {
						curUser = check_credentials_message(users, usersAmount,
								&message);

						if (curUser == NULL) {
							res = send_empty_message(clientSocket,
									CredentialsDeny);
						} else {
							res = send_empty_message(clientSocket,
									CredentialsAccept);
						}

						if (res == -1) {
							print_error();
							break;
						}
					} else if (message.messageType == ShowInbox) {
						if ((prepare_message_from_inbox_content(curUser, &message) == -1) ||
								(send_message(clientSocket, &message, &len) == -1)) {
							print_error();
						}
					}
				} while (1);
			}

			curUser = NULL;
			if (message.data != NULL) {
				free(message.data);
				message.data = NULL;
			}

			if (message.data != NULL) {
				free(message.data);
				message.data = NULL;
			}
			close(clientSocket);
		}
	} while (1);

	/* Releasing resources */
	free_users_array(users, usersAmount);
	close(listenSocket);
	return 0;
}
