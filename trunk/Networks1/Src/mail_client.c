/* system parameters */
#define MAX_HOST_NAME_LEN 256
#define MAX_INPUT_LEN 256
#define MAX_PATH_LEN 200
#define MAX_PORT_LEN 7
#define DEFAULT_HOST_NAME "localhost"
#define DEFAULT_PORT "6423"

/* errors messages definitions */
#define CLIENT_USAGE_MESSAGE "Usage mail_client [hostname [port]]"
#define CREDENTIALS_USAGE_MESSAGE "Expected:\nUser: [username]\nPassword: [password]"
#define WRONG_CREDENTIALS_MESSAGE "Wrong credentials"

/* commands definitions */
#define QUIT_MESSAGE "QUIT\n"
#define SHOW_INBOX "SHOW_INBOX\n"
#define GET_MAIL "GET_MAIL "
#define GET_ATTACHMENT "GET_ATTACHMENT "

/* general definitions */
#define CONNECTION_SUCCEED "Connected to server"

#include "common.h"

/* return ERROR on error
 *         1 on accept
 *		   0 on reject */
int recv_credentials_result(int sourceSocket, Message *message) {
	int res;

	res = recv_message(sourceSocket, message);
	if (res != 0) {
		return res;
	} else if (message->messageType == CredentialsAccept) {
		return 1;
	} else if (message->messageType == CredentialsDeny) {
		return 0;
	} else {
		return ERROR;
	}
}

int prepare_message_from_credentials(char *userName, char *password,
		Message *message) {

	char credentials[MAX_NAME_LEN + MAX_PASSWORD_LEN + 2];
	sprintf(credentials, "%s\t%s", userName, password);

	message->messageType = Credentials;
	message->dataSize = strlen(credentials);
	message->data = (unsigned char*) calloc(message->dataSize, 1);
	if (message->data == NULL) {
		return (ERROR);
	} else {
		memcpy(message->data, credentials, message->dataSize);
		return (0);
	}
}

int send_credentials(char* userName, char* password, int sourceSocket,
		Message *message) {

	if (prepare_message_from_credentials(userName, password, message) != 0) {
		return (ERROR);
	} else {
		return (send_message(sourceSocket, message));
	}
}

int send_show_inbox(int sourceSocket) {
	return send_empty_message(sourceSocket, ShowInbox);
}

int check_typed_message(int sourceSocket, Message *message,
		MessageType messageType) {
	int res;

	res = recv_message(sourceSocket, message);
	if ((res == 0) && (message->messageType == messageType)) {
		return 0;
	} else {
		return ERROR;
	}
}

int recv_inbox_info(int sourceSocket, Message *message) {
	return (check_typed_message(sourceSocket, message, InboxContent));
}

int recv_mail_message(int sourceSocket, Message *message) {
	return (check_typed_message(sourceSocket, message, MailContent));
}

int recv_attachment_message(int sourceSocket, Message *message) {
	return (check_typed_message(sourceSocket, message, AttachmentContent));
}

int prepare_mail_header_from_message(Message *message, Mail *mail, int *offset) {
	int senderLength, subjectLength;

	/* Prepare sender */
	senderLength = strlen((char*) (message->data + *offset)) + 1;
	mail->sender = (char*) calloc(senderLength, 1);
	if (mail->sender == NULL) {
		return (ERROR);
	}
	memcpy(mail->sender, message->data + *offset, senderLength);
	*offset += senderLength;

	/* Prepare subject */
	subjectLength = strlen((char*) (message->data + *offset)) + 1;
	mail->subject = (char*) calloc(subjectLength, 1);
	if (mail->subject == NULL) {
		return (ERROR);
	}
	memcpy(mail->subject, message->data + *offset, subjectLength);
	*offset += subjectLength;

	/* Prepare number of attachments */
	memcpy(&(mail->numAttachments), message->data + *offset,
			sizeof(mail->numAttachments));
	*offset += sizeof(mail->numAttachments);

	return (0);
}

int print_mail_content_from_message(Message *message) {
	int offset = 0, i;
	Mail mail;
	int attachmentNameLen, recipientLen, bodyLength;

	memset(&mail, 0, sizeof(mail));

	if (prepare_mail_header_from_message(message, &mail, &offset) != 0) {
		free_mail_struct(&mail);
		return (ERROR);
	}

	/* Preparing attachments names */
	mail.attachments = calloc(mail.numAttachments, sizeof(Attachment));
	if (mail.attachments == NULL) {
		free_mail_struct(&mail);
		return (ERROR);
	}
	for (i = 0; i < mail.numAttachments; i++) {
		attachmentNameLen = strlen((char*) (message->data + offset)) + 1;
		mail.attachments[i].fileName = calloc(attachmentNameLen, 1);
		if (mail.attachments[i].fileName == NULL) {
			free_mail_struct(&mail);
			return (ERROR);
		}
		memcpy(mail.attachments[i].fileName, message->data + offset,
				attachmentNameLen);
		offset += attachmentNameLen;
	}

	/* Preparing recipients names */
	memcpy(&(mail.numRecipients), message->data + offset,
			sizeof(mail.numRecipients));
	offset += sizeof(mail.numRecipients);
	mail.recipients = calloc(mail.numRecipients, sizeof(char*));
	if (mail.recipients == NULL) {
		free_mail_struct(&mail);
		return (ERROR);
	}
	for (i = 0; i < mail.numRecipients; i++) {
		recipientLen = strlen((char*) (message->data + offset)) + 1;
		mail.recipients[i] = calloc(recipientLen, 1);
		if (mail.recipients[i] == NULL) {
			free_mail_struct(&mail);
			return (ERROR);
		}
		memcpy(mail.recipients[i], message->data + offset, recipientLen);
		offset += recipientLen;
	}

	/* Prepare body */
	bodyLength = strlen((char*) (message->data + offset)) + 1;
	mail.body = (char*) calloc(bodyLength, 1);
	if (mail.body == NULL) {
		free_mail_struct(&mail);
		return (ERROR);
	}
	memcpy(mail.body, message->data + offset, bodyLength);
	offset += bodyLength;

	/* Printing mail content */
	printf("From: %s\nTo: ", mail.sender);
	for (i = 0; i < mail.numRecipients; i++) {
		if (i > 0) {
			printf(",");
		}
		printf("%s", mail.recipients[i]);
	}

	printf("\nSubject: %s\nAttachments: ", mail.subject);
	for (i = 0; i < mail.numAttachments; i++) {
		if (i > 0) {
			printf(",");
		}
		printf("%s", mail.attachments[i].fileName);
	}

	printf("\nText: %s\n", mail.body);

	free_mail_struct(&mail);

	return (0);
}

int print_inbox_info_from_message(Message *message) {
	int offset = 0;
	Mail mail;

	while (message->dataSize > offset) {
		memset(&mail, 0, sizeof(mail));

		/* Prepare id */
		memcpy(&(mail.id), message->data + offset, sizeof(mail.id));
		offset += sizeof(mail.id);

		if (prepare_mail_header_from_message(message, &mail, &offset) != 0) {
			free_mail_struct(&mail);
			return (ERROR);
		}

		printf("%d %s \"%s\" %d\n", mail.id, mail.sender, mail.subject,
				mail.numAttachments);
		free_mail_struct(&mail);
	}

	return (0);
}

int send_get_mail_message(int clientSocket, unsigned short mailID,
		Message* message) {

	message->messageType = GetMail;
	message->dataSize = sizeof(mailID);

	message->data = calloc(message->dataSize, 1);
	if (message->data == NULL) {
		return (ERROR);
	}
	memcpy(message->data, &mailID, message->dataSize);

	return (send_message(clientSocket, message));
}

int send_get_attachment_message(int clientSocket, unsigned short mailID,
		unsigned char attachmentID, Message* message) {
	message->messageType = GetAttachment;
	message->dataSize = sizeof(mailID) + 1;

	message->data = calloc(message->dataSize, 1);
	if (message->data == NULL) {
		return (ERROR);
	}
	memcpy(message->data, &mailID, sizeof(mailID));
	memcpy(message->data + sizeof(mailID), &attachmentID, 1);

	return (send_message(clientSocket, message));
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

	writenBytes = fwrite(message->data + fileNameLength, 1, message->dataSize - fileNameLength, file);
	if (writenBytes != message->dataSize - fileNameLength) {
		fclose(file);
		free(path);
		return(ERROR);
	}

	fclose(file);
	free(path);
	return (0);
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
	char attachmentPath[MAX_PATH_LEN + 1];
	int isLoggedIn = 0;
	unsigned short mailID, tempAttachmentID;
	unsigned char attachmentID;

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
	res = recv_message(clientSocket, &message);

	if (res != 0 || message.messageType != String) {
		close(clientSocket);
		freeaddrinfo(servinfo);
		free_message(&message);
		if (res == ERROR) {
			print_error();
		} else if (res == ERROR_LOGICAL) {
			print_error_message(INVALID_DATA_MESSAGE);
		} else {
			print_error_message(INVALID_DATA_MESSAGE);
		}
		return ERROR;
	} else {
		prepare_string_from_message(&stringMessage, &message);
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
				res = send_credentials(userName, password, clientSocket,
						&message);
				if (res == ERROR) {
					print_error();
					break;
				} else if (res == ERROR_LOGICAL) {
					print_error_message(INVALID_DATA_MESSAGE);
					break;
				}

				res = recv_credentials_result(clientSocket, &message);
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
			fgets(input, MAX_INPUT_LEN, stdin); /*flushing*/
		} else if (strcmp(input, SHOW_INBOX) == 0) {
			res = send_show_inbox(clientSocket);
			if (res == ERROR) {
				print_error();
				break;
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
				break;
			}

			if ((recv_inbox_info(clientSocket, &message) != 0)
					|| (print_inbox_info_from_message(&message) != 0)) {
				print_error();
				break;
			}
		} else if (sscanf(input, GET_MAIL "%hu", &mailID) == 1) {
			res = send_get_mail_message(clientSocket, mailID, &message);
			if (res == ERROR) {
				print_error();
				break;
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
				break;
			}
			free_message(&message);

			res = recv_mail_message(clientSocket, &message);
			if (res != 0) {
				if (message.messageType == InvalidID) {
					print_error_message("Invalid id requested");
				} else {
					print_error();
					break;
				}
			} else if (print_mail_content_from_message(&message) != 0) {
				print_error();
				break;
			}
		} else if (sscanf(input, GET_ATTACHMENT "%hu %hu \"%[^\"]\"", &mailID,
				&tempAttachmentID, attachmentPath) == 3) {
			attachmentID = (unsigned char)(tempAttachmentID & 0xFF);
			res = send_get_attachment_message(clientSocket, mailID,
					attachmentID, &message);
			if (res == ERROR) {
				print_error();
				break;
			} else if (res == ERROR_LOGICAL) {
				print_error_message(INVALID_DATA_MESSAGE);
				break;
			}
			free_message(&message);

			res = recv_attachment_message(clientSocket, &message);
			if (res != 0) {
				if (message.messageType == InvalidID) {
					print_error_message("Invalid id requested");
				} else {
					print_error();
					break;
				}
			} else if (save_attachment_from_message(&message, attachmentPath) != 0) {
				print_error();
				break;
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

