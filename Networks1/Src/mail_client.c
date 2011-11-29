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

int save_file_from_attachment(Attachment *attachment, char *savePath) {

	FILE *file;
	char *path;
	int pathLength;
	size_t writenBytes;

	/* Preparing full path */
	pathLength = strlen(attachment->fileName) + strlen(savePath) + 1;
	path = (char*)calloc(pathLength, 1);
	if (path == NULL) {
		return (ERROR);
	}
	strcat(path, savePath);
	strcat(path, attachment->fileName);

	file = fopen(path, "w");
	if (file == NULL) {
		free(path);
		return(ERROR);
	}

	writenBytes = fwrite(attachment->data, 1, attachment->size, file);
	if (writenBytes != attachment->size) {
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

int count_occurrences(char *str, char chr) {

	int i, occ = 0, length = strlen(str);

	for (i = 0; i < length; i++) {
		if (str[i] == chr) {
			occ++;
		}
	}

	return occ;
}

int insert_file_data_to_attachment(Attachment *attachment, char* path) {

	/* TODO: convert path to absolute */
	char* temp;
	FILE* file = get_valid_file(path);
	int readBytes;

	if (file == NULL) {
		return (ERROR);
	}

	memset(attachment, 0 , sizeof(Attachment));

	/* Preparing size */
	attachment->size = fseek(file, 0, SEEK_END);
	fseek(file, 0, SEEK_SET);

	/* Preparing file name */
	temp = strrchr(path, '/') + 1;
	attachment->fileName = (char*)calloc(strlen(temp) + 1, 1);
	if (attachment->fileName == NULL) {
		free_attachment(attachment);
		return (ERROR);
	}
	strncpy(attachment->fileName, temp, strlen(temp));

	/* Preparing data */
	attachment->data = (unsigned char*)calloc(attachment->size, 1);
	if (attachment->data == NULL) {
		free_attachment(attachment);
		return (ERROR);
	}
	readBytes = fread(attachment->data, 1, attachment->size, file);
	if (readBytes != attachment->size) {
		free_attachment(attachment);
		return (ERROR);
	}

	return (0);
}

int prepare_mail_from_compose_input(Mail *mail, char *curUser, char *tempRecipients, char *tempSubject, char *tempAttachments, char *tempText) {

	int i, res;
	char* temp;

	memset(mail, 0, sizeof(mail));

	/* Preparing sender */
	mail->sender = calloc(strlen(curUser) + 1, 1);
	if (mail->sender == NULL) {
		return (ERROR);
	}
	strncpy(mail->sender, curUser, strlen(curUser));

	/* Preparing subject */
	mail->subject = calloc(strlen(tempSubject) + 1, 1);
	if (mail->subject == NULL) {
		free_mail(mail);
		return (ERROR);
	}
	strncpy(mail->subject, tempSubject, strlen(tempSubject));

	/* Preparing text */
	mail->body = calloc(strlen(tempText) + 1, 1);
	if (mail->body == NULL) {
		free_mail(mail);
		return (ERROR);
	}
	strncpy(mail->body, tempText, strlen(tempText));

	/* Preparing recipients */
	mail->numRecipients = count_occurrences(tempRecipients, ',') + 1;
	mail->recipients = (char**)calloc(mail->numRecipients, sizeof(char*));
	if (mail->recipients == NULL) {
		free_mail(mail);
		return (ERROR);
	}

	temp = strtok(tempRecipients, " ,");
	if (temp == NULL) {
		free_mail(mail);
		return (ERROR);
	}

	for (i = 0; i < mail->numRecipients; i++) {
		mail->recipients[i] = calloc(strlen(temp) + 1, 1);
		if (mail->recipients[i] == NULL) {
			free_mail(mail);
			return (ERROR);
		}
		strncpy(mail->recipients[i], temp, strlen(temp));

		temp = strtok(NULL, " ,");
	}

	/* Preparing attachments */
	mail->numAttachments = count_occurrences(tempRecipients, '"') / 2;

	if (mail->numAttachments > 0) {
		mail->attachments = (Attachment*) calloc(mail->numAttachments,
				sizeof(Attachment));
		if (mail->attachments == NULL) {
			free_mail(mail);
			return (ERROR);
		}

		temp = strtok(tempAttachments, "\",");
		if (temp == NULL) {
			free_mail(mail);
			return (ERROR);
		}

		for (i = 0; i < mail->numAttachments; i++) {
			res = insert_file_data_to_attachment(mail->attachments + i, temp);
			if (res == ERROR) {
				free_mail(mail);
				return (ERROR);
			}

			temp = strtok(NULL, " ,");
		}
	}
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
	res = handle_return_value(res);

	if (res == ERROR) {
		close(clientSocket);
		freeaddrinfo(servinfo);
		return ERROR;
	} else {
		printf("%s\n", stringMessage);
		free(stringMessage);
	}

	do {
		fgets(input, MAX_INPUT_LEN, stdin);
		if (strcmp(input, QUIT_MESSAGE) == 0) {
			res = send_empty_message(clientSocket, Quit);
			res = handle_return_value(res);
			break;
		} else if (!isLoggedIn) {
			if ((sscanf(input, "User: %s", userName) + scanf("Password: %s",
					password)) != 2) {
				print_error_message(CREDENTIALS_USAGE_MESSAGE);
			} else {
				res = send_message_from_credentials(clientSocket, userName, password);
				res = handle_return_value(res);
				if (res == ERROR) {
					break;
				}

				res = recv_credentials_result(clientSocket, &isLoggedIn);
				res = handle_return_value(res);
				if (res == ERROR) {
					break;
				} else {
					if (isLoggedIn) {
						printf("Connected to server\n");
					} else {
						print_error_message(WRONG_CREDENTIALS_MESSAGE);
					}
				}
			}
			/* Flushing */
			fgets(input, MAX_INPUT_LEN, stdin);
		} else if (strcmp(input, SHOW_INBOX) == 0) {
			res = send_empty_message(clientSocket, ShowInbox);
			res = handle_return_value(res);
			if (res == ERROR) {
				break;
			}

			mailAmount = -1;
			mails = NULL;
			res = recv_inbox_content_from_message(clientSocket, &mails, &mailAmount);
			res = handle_return_value(res);
			if (res == ERROR) {
				if (mails != NULL) {
					free_mails(mailAmount, mails);
					free(mails);
				}
				break;
			}

			print_inbox_info(mailAmount, mails);
			free_mails(mailAmount, mails);
		} else if (sscanf(input, GET_MAIL "%hu", &mailID) == 1) {
			res = send_get_mail_message(clientSocket, mailID);
			res = handle_return_value(res);
			if (res == ERROR) {
				break;
			}

			res = recv_mail_from_message(clientSocket, &mail);
			res = handle_return_value(res);
			if (res == ERROR) {
				free_mail(&mail);
				break;
			} else if (res == ERROR_INVALID_ID) {
				free_mail(&mail);
				continue;
			} else {
				print_mail(&mail);
				free_mail(&mail);
			}
		} else if (sscanf(input, GET_ATTACHMENT "%hu %hu \"%[^\"]\"", &mailID,
				&tempAttachmentID, attachmentPath) == 3) {
			attachmentID = (unsigned char)(tempAttachmentID & 0xFF);
			res = send_get_attachment_message(clientSocket, mailID,
					attachmentID);
			res = handle_return_value(res);
			if (res == ERROR) {
				break;
			}

			res = recv_attachment_from_message(clientSocket, &attachment);
			res = handle_return_value(res);
			if (res == ERROR) {
				free_mail(&mail);
				break;
			} else if (res == ERROR_INVALID_ID) {
				free_mail(&mail);
				continue;
			} else {
				if (save_file_from_attachment(&attachment, attachmentPath) != 0) {
					free_attachment(&attachment);
					print_error();
					break;
				} else {
					printf("‫‪Attachment saved‬‬\n");
				}

				free_attachment(&attachment);
			}
		} else if (sscanf(input, DELETE_MAIL "%hu", &mailID) == 1) {
			res = send_delete_mail_message(clientSocket, mailID);
			res = handle_return_value(res);
			if (res == ERROR) {
				break;
			}

			res = recv_delete_result(clientSocket);
			res = handle_return_value(res);
			if (res == ERROR) {
				break;
			}
		} else if (strcmp(input, COMPOSE) == 0) {
			if ((scanf("To: %[^\n]", tempRecipients) != 1) ||
					(scanf("Subject: %[^\n]", tempSubject) != 1) ||
					(scanf("Attachments: %[^\n]", tempAttachments) != 1) ||
					(scanf("Text: %[^\n]", tempText) != 1)) {
				print_error_message(COMPOSE_USAGE_MESSAGE);
			} else {
				res = prepare_mail_from_compose_input(&mail, userName, tempRecipients, tempSubject, tempAttachments, tempText);
				/* TODO: need to send attachments also */
				res = send_message_from_mail(clientSocket, &mail);
				res = handle_return_value(res);
				if (res == ERROR) {
					break;
				}
			}
		} else {
			print_error_message("Invalid command");
		}
	} while (1);

	/* Close connection and socket */
	close(clientSocket);
	freeaddrinfo(servinfo);

	return (0);
}

