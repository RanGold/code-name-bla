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
#define COMPOSE_USAGE_MESSAGE "Expected:\nTo: <username,...>\nSubject: <subject>\nAttachments: [\"path\",..]\nText: <text>"
#define INVALID_COMMAND_MESSAGE "Invalid command"

/* Commands definitions */
#define QUIT_MESSAGE "QUIT\n"
#define SHOW_INBOX "SHOW_INBOX\n"
#define GET_MAIL "GET_MAIL "
#define GET_ATTACHMENT "GET_ATTACHMENT "
#define DELETE_MAIL "DELETE_MAIL "
#define COMPOSE "COMPOSE\n"

/* General Messages */
#define CONNECTION_SUCCEED_MESSAGE "Connected to server\n"
#define ATTACHMENT_SAVE_MESSAGE "‫‪Attachment saved‬‬\n"
#define MAIL_SENT_MESSAGE "Mail sent\n"

#include "common.h"
#include "protocol.h"

/* Print inbox headers data to stdin */
void print_inbox_info(int mailAmount, Mail *mails) {

	int i;

	for (i = 0; i < mailAmount; i++) {
		printf("%d %s \"%s\" %d\n", mails[i].clientId, mails[i].sender, mails[i].subject,
				mails[i].numAttachments);
	}
}

/* Print a mail struct data to the stdin */
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

/* count the number of occurrences of chr is str */
int count_occurrences(char *str, char chr) {

	int i, occ = 0, length = strlen(str);

	for (i = 0; i < length; i++) {
		if (str[i] == chr) {
			occ++;
		}
	}

	return occ;
}

/* Inserting a file data to an attachment */
/* The whole file is inserted to an attachment because the use of file system */
/* is a part of the client implementation, and the the protocol which only recognize */
/* the attachment data structure */
int insert_file_header_to_attachment(Attachment *attachment, char* path) {

	char* temp;
	FILE* file = get_valid_file(path, "r");

	if (file == NULL) {
		return (ERROR);
	}

	memset(attachment, 0 , sizeof(Attachment));

	/* Preparing size */
	fseek(file, 0, SEEK_END);
	attachment->size = ftell(file);
	fseek(file, 0, SEEK_SET);

	/* Preparing file name */
	temp = strrchr(path, '/') + 1;
	attachment->fileName = (char*)calloc(strlen(temp) + 1, 1);
	if (attachment->fileName == NULL) {
		fclose(file);
		free_attachment(attachment);
		return (ERROR);
	}
	strncpy(attachment->fileName, temp, strlen(temp));

	/* Preparing data */
	attachment->data = NULL;
	attachment->file = file;

	return (0);
}

/* Inserting the inputed (valid) compose input to a mail struct including attachments files data */
int prepare_mail_from_compose_input(Mail *mail, char *curUser,
		char *tempRecipients, char *tempSubject, char *tempAttachments,
		char *tempText) {

	int i, res;
	char* temp;

	memset(mail, 0, sizeof(mail));

	/* Preparing empty sender (because server is aware of current sender */
	mail->sender = calloc(1, 1);
	if (mail->sender == NULL) {
		free_mail(mail);
		return (ERROR);
	}

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
	mail->numAttachments = count_occurrences(tempAttachments, '"') / 2;

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
			res = insert_file_header_to_attachment(mail->attachments + i, temp);
			if (res == ERROR) {
				free_mail(mail);
				return (ERROR);
			}

			temp = strtok(NULL, "\",");
		}
	}
	return (0);
}

int main(int argc, char** argv) {

	/* Variables declaration */
	char hostname[MAX_HOST_NAME_LEN + 1] = DEFAULT_HOST_NAME;
	char portString[MAX_PORT_LEN + 1] = DEFAULT_PORT;
	int clientSocket,chatSocket;
	struct addrinfo hints, *servinfo;
	int res;
	char *stringMessage;
	char userName[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	char input[MAX_INPUT_LEN + 1];
	unsigned short mailAmount;
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
		return (ERROR);
	}

	/* Connect to server */
	clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	res = connect(clientSocket, servinfo->ai_addr, servinfo->ai_addrlen);
	if (res == ERROR) {
		print_error();
		freeaddrinfo(servinfo);
		return (ERROR);
	}

	chatSocket = socket(PF_INET, SOCK_STREAM, 0);
	res = connect(chatSocket, servinfo->ai_addr, servinfo->ai_addrlen);
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
		return (ERROR);
	} else {
		printf("%s\n", stringMessage);
		free(stringMessage);
	}

	res = recv_string_from_message(chatSocket, &stringMessage);
	res = handle_return_value(res);
	if (res == ERROR) {
		close(chatSocket);
		freeaddrinfo(servinfo);
		return (ERROR);
	} else {
		free(stringMessage);
	}

	/* Initializing structs */
	memset(&mail, 0, sizeof(Mail));
	memset(&attachment, 0, sizeof(Attachment));
	mails = NULL;

	do {
		res = 0;
		fgets(input, MAX_INPUT_LEN, stdin);
		if (strcmp(input, QUIT_MESSAGE) == 0) {
			res = send_quit_message(clientSocket);
			res = handle_return_value(res);
			break;
		} else if (!isLoggedIn) {
			if ((sscanf(input, "User: %s", userName) +
					scanf("Password: %s", password)) != 2) {
				print_error_message(CREDENTIALS_USAGE_MESSAGE);
			} else {
				res = send_message_from_credentials(clientSocket, chatSocket, userName, password);
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
						printf(CONNECTION_SUCCEED_MESSAGE);
					} else {
						print_error_message(WRONG_CREDENTIALS_MESSAGE);
					}
				}
			}
			/* Flushing */
			fgets(input, MAX_INPUT_LEN, stdin);
		} else if (strcmp(input, SHOW_INBOX) == 0) {
			res = send_show_inbox_message(clientSocket);
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
			free(mails);
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

			/* Converting to unsigned char while zeroing the non important bits */
			attachmentID = (unsigned char)tempAttachmentID;
			res = send_get_attachment_message(clientSocket, mailID,
					attachmentID);
			res = handle_return_value(res);
			if (res == ERROR) {
				break;
			}

			res = recv_attachment_file_from_message(clientSocket, &attachment, attachmentPath);
			res = handle_return_value(res);
			if (res == ERROR) {
				free_mail(&mail);
				break;
			} else if (res == ERROR_INVALID_ID) {
				free_mail(&mail);
				res = 0;
				continue;
			} else {
				printf(ATTACHMENT_SAVE_MESSAGE);
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
			/* Checking input */
			res = 0;
			fgets(input, MAX_INPUT_LEN, stdin);
			if (sscanf(input, "To: %[^\n]\n", tempRecipients) == 1) {
				fgets(input, MAX_INPUT_LEN, stdin);
				if (sscanf(input, "Subject: %[^\n]\n", tempSubject) == 1) {
					fgets(input, MAX_INPUT_LEN, stdin);
					if ((sscanf(input, "Attachments: %[^\n]\n", tempAttachments) == 1) ||
							(sscanf(input, "Attachments:%[\n]", tempAttachments) == 1) ||
							(sscanf(input, "Attachments:%*[ ]%[\n]", tempAttachments) == 1)){
						fgets(input, MAX_INPUT_LEN, stdin);
						if (sscanf(input, "Text: %[^\n]\n", tempText) == 1) {
							res = 1;
						}
					}
				}
			}

			if (res == 0) {
				print_error_message(COMPOSE_USAGE_MESSAGE);
			} else {
				res = prepare_mail_from_compose_input(&mail, userName, tempRecipients,
						tempSubject, tempAttachments, tempText);
				res = handle_return_value(res);
				if (res == ERROR) {
					break;
				}

				res = send_compose_message_from_mail(clientSocket, &mail);
				res = handle_return_value(res);
				if (res == ERROR) {
					free_mail(&mail);
					break;
				}
				free_mail(&mail);

				res = recv_send_result(clientSocket);
				res = handle_return_value(res);
				if (res == ERROR) {
					break;
				} else {
					printf(MAIL_SENT_MESSAGE);
				}
			}
		} else {
			print_error_message(INVALID_COMMAND_MESSAGE);
		}
	} while (1);

	/* Close connection and socket */
	close(clientSocket);
	freeaddrinfo(servinfo);

	return (res);
}
