#define MAX_ROW_LENGTH 256
#define WELLCOME_MESSAGE "Welcome! I am simple-mail-server."
#define SERVER_USAGE_MSG "Usage mail_server <users_file> [port]"

#include "common.h"

void create_stub(User *user){
	Mail mail1, mail2, mail_deleted;
	mail1.id = 1;
	mail1.sender = "ran";
	mail1.subject = "test";
	mail1.numAttachments = 2;

	mail2.id = 2;
	mail2.sender = "amir";
	mail2.subject = "test2";
	mail2.numAttachments = 1;

	mail_deleted.id = -1;

	user->mails = calloc(3, sizeof(Mail));
	user->mails[0] = mail1;
	user->mails[1] = mail_deleted;
	user->mails[2] = mail2;
	user->mailAmount = 3;

}

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

int calculate_inbox_info_size(User *user){
	int i, messageSize =0;
	for (i = 0; i < user->mailAmount; i++) {
		if (user->mails[i].id != -1){
			messageSize += sizeof(short);
			messageSize += strlen(user->mails[i].sender);
			messageSize += strlen(user->mails[i].subject);
			messageSize += sizeof(unsigned char);
			messageSize += (2 * sizeof(short));  /* +2 for 2 short numbers for lengths */
		}
	}
	return messageSize;
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

	int i, j;

	for (i = 0; i < usersAmount; i++) {
		for (j = 0; j < users[i].mailAmount; j++) {
			free_mail_struct(users[i].mails + j);
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

	int i, messageSize = 0;
	char temp[100];
	char *messageText;
	int placeToCopy =0;
	short senderLength, subjectLength;

	message->messageType = InboxContent;

	/* Calculating message size */
	messageSize = calculate_inbox_info_size(user);

	messageText = (char*)calloc(messageSize, 1);
	if (messageText == NULL) {
		return (-1);
	}

	for (i = 0; i < user->mailAmount; i++) {
		if (user->mails[i].id != -1){
			senderLength = strlen(user->mails[i].sender) + 1;
			subjectLength = strlen(user->mails[i].subject) + 1;

			memcpy(messageText + placeToCopy, &user->mails[i].id, sizeof(short));
			placeToCopy += sizeof(short);
			memcpy(messageText + placeToCopy, &senderLength, sizeof(short));
			placeToCopy += sizeof(short);
			memcpy(messageText + placeToCopy, user->mails[i].sender, senderLength);
			placeToCopy += senderLength;
			memcpy(messageText + placeToCopy, &subjectLength, sizeof(short));
			placeToCopy += sizeof(short);
			memcpy(messageText + placeToCopy, user->mails[i].subject, subjectLength);
			placeToCopy += subjectLength;
			memcpy(messageText + placeToCopy, &user->mails[i].numAttachments, sizeof(unsigned char));
			placeToCopy += sizeof(unsigned char);
		}
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
		print_error_message(SERVER_USAGE_MSG);
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
							create_stub(curUser);
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
