/* System parameters */
#define MAX_HOST_NAME_LEN 256
#define MAX_INPUT_LEN 1024
#define MAX_PATH_LEN 256
#define MAX_PORT_LEN 7
#define DEFAULT_HOST_NAME "localhost"
#define DEFAULT_PORT "6423"

/* Errors messages definitions */
#define CLIENT_USAGE_MESSAGE "Usage mail_client [hostname [port]]"
#define CREDENTIALS_USAGE_MESSAGE "Expected:\nUser: [username]\nPassword: [password]"
#define WRONG_CREDENTIALS_MESSAGE "Wrong credentials"
#define COMPOSE_USAGE_MESSAGE "Expected:\nTo: [username,...]\nSubject: [subject]\nAttachments: [path,..]\nText: [text]"
#define INVALID_ID_MESSAGE "Invalid id requested"

/* Commands definitions */
#define QUIT_MESSAGE "QUIT\n"
#define SHOW_INBOX "SHOW_INBOX\n"
#define GET_MAIL "GET_MAIL "
#define GET_ATTACHMENT "GET_ATTACHMENT "
#define DELETE_MAIL "DELETE_MAIL "
#define COMPOSE "COMPOSE\n"

/* General definitions */
#define CONNECTION_SUCCEED "Connected to server"

#include "common.h"

/* return ERROR on error
 *         1 on accept
 *		   0 on reject */
int recv_credentials_result(int sourceSocket) {

	int res;
	Message *message;

	res = recv_message(sourceSocket, message);
	if (res != 0) {
	} else if (message->messageType == CredentialsAccept) {
		res = 1;
	} else if (message->messageType == CredentialsDeny) {
		res = 0;
	} else {
		res = ERROR;
	}

	free_message(message);
	return(res);
}

int send_show_inbox(int sourceSocket) {
	return send_empty_message(sourceSocket, ShowInbox);
}

int send_delete_mail_message(int clientSocket, unsigned short mailID,
		Message* message) {

	return (send_mail_id_message(clientSocket, mailID, message, DeleteMail));
}

int save_attachment_from_message(Message *message, char *savePath) {
	FILE *file;
	char *fileName, *path;
	int fileNameLength, pathLength;
	size_t writenBytes;

	/* Getting the attachment's name from the message */
	fileNameLength = strlen((char*)message->data) + 1;
	fileName = (char*)calloc(fileNameLength, 1);
	if (fileName == NULL) {
		return (ERROR);
	}
	memcpy(fileName, message->data, fileNameLength);

	/* Preparing full path */
	pathLength = strlen(fileName) + strlen(savePath) + 1;
	path = (char*)calloc(pathLength, 1);
	if (path == NULL) {
		free(fileName);
		return (ERROR);
	}
	strcat(path, savePath);
	strcat(path, fileName);
	free(fileName);

	file = fopen(path, "w");
	if (file == NULL) {
		free(path);
		return(ERROR);
	}

	writenBytes = fwrite(message->data + fileNameLength, 1, message->size - fileNameLength, file);
	if (writenBytes != message->size - fileNameLength) {
		fclose(file);
		free(path);
		return(ERROR);
	}

	fclose(file);
	free(path);
	return (0);
}

void print_inbox_info(int mailAmount, Mail *mails) {

	int i;

	for (i = 0; i < mailAmount; i++) {
		printf("%d %s \"%s\" %d\n", mails[i].id, mails[i].sender, mails[i].subject,
				mails[i].numAttachments);
	}
}

void print_mail(Mail *mail) {

	int i;

	/* Printing mail content */
	printf("From: %s\nTo: ", mail->sender);
	for (i = 0; i < mail->numRecipients; i++) {
		if (i > 0) {
			printf(",");
		}
		printf("%s", mail->recipients[i]);
	}

	printf("\nSubject: %s\nAttachments: ", mail->subject);
	for (i = 0; i < mail->numAttachments; i++) {
		if (i > 0) {
			printf(",");
		}
		printf("%s", mail->attachments[i].fileName);
	}

	printf("\nText: %s\n", mail->body);
}

/* TODO: make sure whenever error this returns -1 */
int main(int argc, char** argv) {

	/* Variables declaration */
	char hostname[MAX_HOST_NAME_LEN + 1] = DEFAULT_HOST_NAME;
	char portString[MAX_PORT_LEN + 1] = DEFAULT_PORT;
	int clientSocket;
	struct addrinfo hints, *servinfo;
	int res;
	Message message;
	char* stringMessage;
	char userName[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	char input[MAX_INPUT_LEN + 1];
	int mailAmount;
	Mail *mails, mail;
	char attachmentPath[MAX_PATH_LEN + 1];
	int isLoggedIn = 0;
	unsigned short mailID, tempAttachmentID;
	unsigned char attachmentID;
	Attachment attachment;
	char tempRecipients[MAX_INPUT_LEN + 1], tempSubject[MAX_INPUT_LEN + 1],
			tempAttachments[MAX_INPUT_LEN + 1], tempText[MAX_INPUT_LEN + 1];

	/* Validate number of arguments */
	if (argc != 1 && argc != 2 && argc != 3) {
		print_error_message(CLIENT_USAGE_MESSAGE);
		return (ERROR);
	} else if (argc == 2) {
		strncpy(hostname, argv[1], MAX_HOST_NAME_LEN);
	} else if (argc == 3) {
		strncpy(hostname, argv[1], MAX_HOST_NAME_LEN);
		strncpy(portString, argv[2], MAX_PORT_LEN);
	}

	/* Checking for address validity */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	res = getaddrinfo(hostname, portString, &hints, &servinfo);
	if (res != 0) {
		fprintf(stderr, "%s\n", gai_strerror(res));
		print_error_message(CLIENT_USAGE_MESSAGE);
		return ERROR;
	}

	clientSocket = socket(PF_INET, SOCK_STREAM, 0);

	/* Connect to server */
	res = connect(clientSocket, servinfo->ai_addr, servinfo->ai_addrlen);
	if (res == ERROR) {
		print_error();
		freeaddrinfo(servinfo);
		return (ERROR);
	}

	/* Receiving welcome message */
	res = recv_string_from_message(clientSocket, &stringMessage);

	if (res != 0) {
		close(clientSocket);
		freeaddrinfo(servinfo);
		if (res == ERROR) {
			print_error();
		} else if (res == ERROR_LOGICAL) {
			print_error_message(INVALID_DATA_MESSAGE);
		} else {
			print_error_message(INVALID_DATA_MESSAGE);
		}
		return ERROR;
	} else {
		printf("%s\n", stringMessage);
		free(stringMessage);
	}

	do {
		fgets(input, MAX_INPUT_LEN, stdin);
		if (strcmp(input, QUIT_MESSAGE) == 0) {
			if (send_empty_message(clientSocket, Quit) == 0) {
				break;
			} else if (res == ERROR) {
				print_error();
				break;
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
				break;
			} else {
				break;
			}
		} else if (!isLoggedIn) {
			if ((sscanf(input, "User: %s", userName) + scanf("Password: %s",
					password)) != 2) {
				print_error_message(CREDENTIALS_USAGE_MESSAGE);
			} else {
				res = send_message_from_credentials(clientSocket, userName, password);
				if (res == ERROR) {
					print_error();
					break;
				} else if (res == ERROR_LOGICAL) {
					print_error_message(INVALID_DATA_MESSAGE);
					break;
				}

				res = recv_credentials_result(clientSocket);
				if (res == 1) {
					isLoggedIn = 1;
					printf("Connected to server\n");
				} else if (res == 0) {
					print_error_message(WRONG_CREDENTIALS_MESSAGE);
				} else if (res == ERROR_LOGICAL) {
					print_error_message(INVALID_DATA_MESSAGE);
					break;
				} else {
					print_error();
					break;
				}
			}
			/* Flushing */
			fgets(input, MAX_INPUT_LEN, stdin);
		} else if (strcmp(input, SHOW_INBOX) == 0) {
			res = send_show_inbox(clientSocket);
			if (res == ERROR) {
				print_error();
				break;
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
				break;
			}

			mailAmount = -1;
			mails = NULL;
			res = recv_inbox_content_from_message(clientSocket, &mails, &mailAmount);
			if (res != 0) {
				free_mails(mailAmount, mails);
				if (mails != NULL) {
					free(mails);
				}
				if (res == ERROR_LOGICAL) {
					print_error_message(INVALID_DATA_MESSAGE);
					break;
				} else {
					print_error();
					break;
				}
			}

			print_inbox_info(mailAmount, mails);
			free_mails(mailAmount, mails);
		} else if (sscanf(input, GET_MAIL "%hu", &mailID) == 1) {
			res = send_get_mail_message(clientSocket, mailID);
			if (res == ERROR) {
				print_error();
				break;
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
				break;
			}

			res = recv_mail_from_message(clientSocket, &mail);
			if (res != 0) {
				free_mail(&mail);
				if (message.messageType == InvalidID) {
					print_error_message(INVALID_ID_MESSAGE);
				} else {
					print_error();
					break;
				}
			}

			print_mail(&mail);
			free_mail(&mail);
		} else if (sscanf(input, GET_ATTACHMENT "%hu %hu \"%[^\"]\"", &mailID,
				&tempAttachmentID, attachmentPath) == 3) {
			attachmentID = (unsigned char)(tempAttachmentID & 0xFF);
			res = send_get_attachment_message(clientSocket, mailID,
					attachmentID);
			if (res == ERROR) {
				print_error();
				break;
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
				break;
			}

			/* TODO: continue refactoring here */
			res = recv_attachment_from_message(clientSocket, &attachment);
			if (res != 0) {
				if (message.messageType == InvalidID) {
					print_error_message(INVALID_ID_MESSAGE);
				} else {
					print_error();
					break;
				}
			} else if (save_attachment_from_message(&message, attachmentPath) != 0) {
				print_error();
				break;
			} else {
				printf ("‫‪Attachment saved‬‬\n");
			}
		} else if (sscanf(input, DELETE_MAIL "%hu", &mailID) == 1) {
			res = send_delete_mail_message(clientSocket, mailID, &message);
			if (res == ERROR) {
				print_error();
				break;
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
				break;
			}
			free_message(&message);

			res = recv_message(clientSocket, &message);
			if (res != 0) {
				print_error();
				break;
			} else {
				if (message.messageType == InvalidID) {
					print_error_message(INVALID_ID_MESSAGE);
				} else if (message.messageType != DeleteApprove) {
					print_error_message("Invalid data received");
					break;
				}
			}
		} else if (strcmp(input, COMPOSE) == 0) {
			if ((scanf("To: %s", tempRecipients) != 1) ||
					(scanf("Subject: %s", tempSubject) != 1) ||
					(scanf("Attachments: %s", tempAttachments) != 1) ||
					(scanf("Text: %s", tempText) != 1)) {
				print_error_message(COMPOSE_USAGE_MESSAGE);
			} else {

			}
		} else {
			print_error_message("Invalid command");
		}

		free_message(&message);
	} while (1);

	/* Close connection and socket */
	/*TODO: maybe free Message data ?*/
	free_message(&message);
	close(clientSocket);
	freeaddrinfo(servinfo);

	return (0);
}

