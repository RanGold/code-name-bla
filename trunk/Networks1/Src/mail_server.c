#define MAX_ROW_LENGTH 256
#define WELLCOME_MESSAGE "Welcome! I am simple-mail-server."
#define SERVER_USAGE_MSG "Usage mail_server <users_file> [port]"
#define INIT_USER_ARR_FAILED "Failed initiallizing users array"
#define DEAFULT_PORT 6423

#include "common.h"
#include "protocol.h"

typedef struct {
	char name[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	int mailsUsed;
	int mailArraySize;
	Mail** mails;
} User;

/* TODO: delete this */
void create_stub(User *user){
	Mail mail1, mail2;
	FILE* file;

	mail1.id = 1;
	mail1.sender = calloc(4, 1);
	strcpy(mail1.sender, "ran");
	mail1.subject = calloc(5, 1);
	strcpy(mail1.subject, "test");
	mail1.body = calloc(strlen("this is a test message"), 1);
	strcpy(mail1.body, "this is a test message");
	mail1.numAttachments = 1;
	mail1.attachments = calloc(1, sizeof(Attachment));
	mail1.attachments[0].size = 27;
	mail1.attachments[0].fileName = calloc(6, 1);
	strcpy(mail1.attachments[0].fileName, "users");
	mail1.attachments[0].data = calloc(27,1);
	file = fopen("/home/student/EclipseWorkspace/Networks1/users", "r");
	fread(mail1.attachments[0].data, 27, 1, file);
	fclose(file);
	mail1.numRecipients = 2;
	mail1.recipients = calloc(2, sizeof(char*));
	mail1.recipients[0] = calloc(5, 1);
	strcpy(mail1.recipients[0], "amir");
	mail1.recipients[1] = calloc(6, 1);
	strcpy(mail1.recipients[1], "tammy");
	mail1.numRefrences = 1;

	mail2.id = 2;
	mail2.sender = calloc(5, 1);
	strcpy(mail2.sender, "amir");
	mail2.subject = calloc(6, 1);
	strcpy(mail2.subject, "test2");
	mail2.numAttachments = 0;
	mail2.numRecipients = 0;
	mail2.body = calloc(strlen("this is a test message"), 1);
	strcpy(mail2.body, "Hey there, sup?");
	mail2.numRefrences = 1;

	user->mails = calloc(4, sizeof(Mail*));
	user->mails[0] = calloc(sizeof(Mail), 1);
	*(user->mails[0]) = mail1;
	user->mails[1] = NULL;
	user->mails[2] = calloc(sizeof(Mail), 1);
	*(user->mails[2]) = mail2;
	user->mailsUsed = 2;
	user->mailArraySize = 4;

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

void free_users_array(User *users, int usersAmount) {

	int i, j;

	for (i = 0; i < usersAmount; i++) {
		for (j = 0; j <= users[i].mailsUsed; j++) {
			if (users[i].mails[j] != NULL) {
				free_mail(users[i].mails[j]);
			}
		}
		free(users[i].mails);
	}

	free(users);
}

int initialliaze_users_array(int* usersAmount, User** users, char* filePath) {

	int i;
	FILE* usersFile;

	/* Get file for reading */
	usersFile = get_valid_file(filePath, "r");
	if (usersFile == NULL) {
		return (ERROR);
	}

	*usersAmount = count_rows(usersFile);
	*users = (User*) calloc(*usersAmount, sizeof(User));

	fseek(usersFile, 0, SEEK_SET);

	for (i = 0; i < *usersAmount; i++) {

		if (fscanf(usersFile, "%s\t%s", (*users)[i].name, (*users)[i].password) != 2) {
			fclose(usersFile);
			free_users_array(*users, *usersAmount);
			return (ERROR);
		}

		(*users)[i].mailsUsed = 0;
		(*users)[i].mailArraySize = 1;
		(*users)[i].mails = calloc(1, sizeof(Mail*));
		if ((*users)[i].mails == NULL) {
			fclose(usersFile);
			free_users_array(*users, *usersAmount);
			return (ERROR);
		}
	}

	fclose(usersFile);
	return (0);
}

User* check_credentials_message(User* users, int usersAmount, Message *message) {

	char userName[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	int i;

	if (prepare_credentials_from_message(message, userName, password) != 0) {
		return (NULL);
	}

	for (i = 0; i < usersAmount; i++) {
		if ((strcmp(users[i].name, userName) == 0) && (strcmp(users[i].password, password) == 0)) {
			return (users + i);
		}
	}

	return (NULL);
}

Mail* get_mail_by_id (User *user, unsigned short mailID, int *mailIndex) {

	int i;

	for (i = 0; i <= user->mailsUsed; i++) {
		if ((user->mails[i] != NULL) && (user->mails[i]->id == mailID)) {
			*mailIndex = i;
			return (user->mails[i]);
		}
	}

	return (NULL);
}

Attachment* get_attachment_by_id (User *user, short mailID, unsigned char attachmentID) {
	int mailIndex;
	Mail* mail = get_mail_by_id(user, mailID, &mailIndex);

	if ((mail != NULL) && (mail->numAttachments >= attachmentID)) {
		return (mail->attachments + attachmentID - 1);
	}

	return (NULL);
}

int delete_mail(User *user, unsigned short mailID) {
	int mailIndex;
	Mail *mail = get_mail_by_id(user, mailID, &mailIndex);

	if (mail == NULL) {
		return (ERROR_INVALID_ID);
	}

	/* Removing reference */
	user->mails[mailIndex] = NULL;
	mail->numRefrences--;

	if (mail->numRefrences == 0) {
		free_mail(mail);
	}

	return (0);
}

int initiallize_listen_socket(int *listenSocket, short port) {

	struct sockaddr_in serverAddr;
	int res;

	/* Create listen socket - needs to be same as the client */
	*listenSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (*listenSocket == -1) {
		return (ERROR);
	}

	/* Prepare address structure to bind listen socket */
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);

	/* Bind listen socket - no other processes can bind to this port */
	res = bind(*listenSocket, (struct sockaddr*) &serverAddr,
					sizeof(serverAddr));
	if (res == -1) {
		close(*listenSocket);
		return (ERROR);
	}

	/* Start listening */
	res = listen(*listenSocket, 1);
	if (res == -1) {
		close(*listenSocket);
		return (ERROR);
	}

	return (0);
}

int add_mail_to_server(User *users, int usersAmount, char *curUserName, Mail *mail) {

	int i, j;

	/* Setting sender */
	mail->sender = calloc(strlen(curUserName) + 1, 1);
	if (mail->sender == NULL) {
		return (ERROR);
	}
	strncpy(mail->sender, curUserName, strlen(curUserName));

	/* Adding mail to recipients */
	/* TODO: what about non exiting recipients? */
	mail->numRefrences = 0;
	for (i = 0; i < usersAmount; i++) {
		for (j = 0; j < mail->numRecipients; j++) {
			if (strcmp(mail->recipients[j], users[i].name) == 0) {
				/* Checking if the mail array should be enlarged */
				if (users[i].mailsUsed == users[i].mailArraySize) {
					users[i].mailArraySize *= 2;
					users[i].mails = realloc(users[i].mails, users[i].mailArraySize);
					if (users[i].mails == NULL) {
						return(ERROR);
					}
				}

				users[i].mails[users[i].mailsUsed] = mail;
				users[i].mailsUsed++;
			}
		}
	}

	return (0);
}

/* TODO: make sure whenever error this returns -1 */
/* TODO: review error situations and actions */
int main(int argc, char** argv) {

	/* Variables declaration */
	short port = DEAFULT_PORT;
	int usersAmount, res, mailIndex;
	unsigned int len;
	User *users = NULL, *curUser = NULL;
	int listenSocket, clientSocket;
	struct sockaddr_in clientAddr;
	unsigned short mailID;
	unsigned char attachmentID;
	Message message;
	Mail *mail;
	Attachment *attachment;

	/* Validate number of arguments */
	if (argc != 2 && argc != 3) {
		print_error_message(SERVER_USAGE_MSG);
		return (ERROR);
	} else if (argc == 3) {
		port = (short) atoi(argv[2]);
	}

	res = initialliaze_users_array(&usersAmount, &users, argv[1]);
	if (res == ERROR) {
		print_error_message(INIT_USER_ARR_FAILED);
		print_error();
		return(ERROR);
	}

	if (initiallize_listen_socket(&listenSocket, port) == ERROR) {
		print_error();
		free_users_array(users, usersAmount);
		return (ERROR);
	}

	memset(&message, 0, sizeof(Message));
	do {

		/* Prepare structure for client address */
		len = sizeof(clientAddr);

		/* Start waiting until client connect */
		clientSocket = accept(listenSocket, (struct sockaddr*) &clientAddr, &len);
		if (clientSocket == -1) {
			print_error();
		} else {
			res = send_message_from_string(clientSocket, WELLCOME_MESSAGE);
			if (res == ERROR) {
				print_error();
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
			} else {
				do {
					res = recv_message(clientSocket, &message);
					if (res != 0) {
						if (res == ERROR) {
							print_error();
						} else if (res == ERROR_LOGICAL) {
							print_error_message(INVALID_DATA_MESSAGE);
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
							/* TODO: delete this */
							create_stub(curUser);
						}

						if (res == ERROR) {
							print_error();
							break;
						} else if (res == ERROR_LOGICAL) {
							print_error_message(INVALID_DATA_MESSAGE);
						}
					} else if (message.messageType == ShowInbox) {
						res = send_message_from_inbox_content(clientSocket, curUser->mails, curUser->mailsUsed);
						if (res == ERROR) {
							print_error();
							break;
						} else if (res == ERROR_LOGICAL) {
							print_error_message(INVALID_DATA_MESSAGE);
						}
					} else if (message.messageType == GetMail) {

						get_mail_id_from_message(&message, &mailID, GetMail);
						if (mailID == ERROR_LOGICAL) {
							print_error_message(INVALID_DATA_MESSAGE);
							break;
						}

						mail = get_mail_by_id(curUser, mailID, &mailIndex);
						if (mail == NULL) {
							send_empty_message(clientSocket, InvalidID);
						} else {
							res = send_message_from_mail(clientSocket, mail);
							if (res == ERROR) {
								print_error();
								break;
							} else if (res == ERROR_LOGICAL) {
								print_error_message(INVALID_DATA_MESSAGE);
							}
						}
					} else if (message.messageType == GetAttachment) {

						get_mail_attachment_id_from_message(&message, &mailID, &attachmentID);
						if (mailID == ERROR_LOGICAL) {
							print_error_message(INVALID_DATA_MESSAGE);
							break;
						}

						attachment = get_attachment_by_id(curUser, mailID,
								attachmentID);
						if (attachment == NULL) {
							send_empty_message(clientSocket, InvalidID);
						} else {
							res = send_message_from_attachment(clientSocket,
									attachment);
							if (res == ERROR) {
								print_error();
								break;
							} else if (res == ERROR_LOGICAL) {
								print_error_message(INVALID_DATA_MESSAGE);
							}
						}
					} else if (message.messageType == DeleteMail) {
						get_mail_id_from_message(&message, &mailID, DeleteMail);
						if (mailID == ERROR_LOGICAL) {
							print_error_message(INVALID_DATA_MESSAGE);
							break;
						}

						res = delete_mail(curUser, mailID);
						if (res == ERROR_INVALID_ID) {
							send_empty_message(clientSocket, InvalidID);
						} else if ((res != 0) || (send_empty_message(clientSocket,
								DeleteApprove) != 0)) {
							print_error();
						}
					} else if (message.messageType == Compose) {
						res = prepare_mail_from_compose_message(&message, &mail);
						if (res != 0) {
							print_error();
							break;
						}

						res = add_mail_to_server(users, usersAmount, curUser->name, mail);
					} else {
						res = send_empty_message(clientSocket, InvalidCommand);
						if (res == ERROR) {
							print_error();
							break;
						}
					}

					free_message(&message);
				} while (1);
			}

			curUser = NULL;
			free_message(&message);
			close(clientSocket);
		}
	} while (1);

	/* Releasing resources */
	free_users_array(users, usersAmount);
	close(listenSocket);
	free_message(&message);

	return (0);
}
