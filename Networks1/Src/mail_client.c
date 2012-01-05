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
	if (tempSubject != NULL){
		mail->subject = calloc(strlen(tempSubject) + 1, 1);
		if (mail->subject == NULL) {
			free_mail(mail);
			return (ERROR);
		}
		strncpy(mail->subject, tempSubject, strlen(tempSubject));
	}

	/* Preparing text */
	if (tempText != NULL){
		mail->body = calloc(strlen(tempText) + 1, 1);
		if (mail->body == NULL) {
			free_mail(mail);
			return (ERROR);
		}
		strncpy(mail->body, tempText, strlen(tempText));
	}
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

int do_get_chat(int chatSocket) {
	Mail mail;
	int res;

	memset(&mail, 0, sizeof(Mail));

	res = recv_chat_from_message(chatSocket, &mail);
	/* TODO: handle error */

	printf("New message from %s:  %s",mail.sender, mail.body);
	free_mail(&mail);
	return (0);
}

int do_quit(int mainSocket, int chatSocket){
	/*TODO: why need res ?? */
	int res;

	res = send_quit_message(mainSocket, -1, NULL);
	res = handle_return_value(res);

	res = send_quit_message(chatSocket, -1, NULL);
	res = handle_return_value(res);

	return (0);
}

int do_credentials(int mainSocket, int chatSocket, int *isLoggedIn, char *userName, char *password,
		fd_set readfds, fd_set errorfds, int *maxFd){
	char input[MAX_INPUT_LEN + 1];
	int res;

	if ((sscanf(input, "User: %s", userName) + scanf("Password: %s", password)) != 2) {
		print_error_message(CREDENTIALS_USAGE_MESSAGE);

	} else {
		res = send_message_from_credentials(mainSocket, chatSocket, userName, password);
		res = handle_return_value(res);

		if (res == ERROR) {
			return (res);
		}

		res = recv_credentials_result(mainSocket, isLoggedIn, -1, NULL);
		res = handle_return_value(res);
		if (res == ERROR) {
			return (res);
		} else {
			if (isLoggedIn) {
				FD_SET(chatSocket, &readfds);
				FD_SET(chatSocket, &errorfds);
				*maxFd = chatSocket;
				printf(CONNECTION_SUCCEED_MESSAGE);
			} else {
				print_error_message(WRONG_CREDENTIALS_MESSAGE);
			}
		}
	}

	return (0);
}

int do_show_inbox(int mainSocket, int chatSocket){
	int res;
	unsigned short mailAmount;
	Mail *mails;

	mails = NULL;
	res = send_show_inbox_message(mainSocket, chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		return (res);
	}

	mailAmount = -1;
	res = recv_inbox_content_from_message(mainSocket, &mails, &mailAmount,
			chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		if (mails != NULL) {
			free_mails(mailAmount, mails);
			free(mails);
		}
		return (res);
	}

	print_inbox_info(mailAmount, mails);
	free_mails(mailAmount, mails);
	free(mails);

	return (0);
}

int do_get_mail(int mainSocket, int chatSocket, unsigned short mailID){
	int res;
	Mail mail;

	res = send_get_mail_message(mainSocket, mailID, chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		return (res);
	}

	res = recv_mail_from_message(mainSocket, &mail, chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		free_mail(&mail);
		return (res);
	} else if (res == ERROR_INVALID_ID) {
		free_mail(&mail);
		return (res);  /* check in main ERROR_INVALID_ID */
	} else {
		print_mail(&mail);
		free_mail(&mail);
	}
	return (0);
}

int do_get_attachment(int mainSocket, int chatSocket, unsigned char attachmentID,
		char * attachmentPath, unsigned short mailID){
	int res;
	Attachment attachment;

	memset(&attachment, 0, sizeof(Attachment));

	/* Converting to unsigned char while zeroing the non important bits */
	res = send_get_attachment_message(mainSocket, mailID, attachmentID, chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		return (res);
	}

	res = recv_attachment_file_from_message(mainSocket, &attachment, attachmentPath,
			chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		return (res);  /* TODO: originally there was free_mail here - why ?? */
	} else {
		printf(ATTACHMENT_SAVE_MESSAGE);
	}
	return (0);
}

int do_delete_mail(int mainSocket, int chatSocket, unsigned short mailID){
	int res;

	res = send_delete_mail_message(mainSocket, mailID, chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		return (res);
	}

	res = recv_delete_result(mainSocket, chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		return (res);
	}

	return (0);
}

int do_compose(int mainSocket, int chatSocket, char *userName){
	int res;
	Mail mail;
	char input[MAX_INPUT_LEN + 1];
	char tempRecipients[MAX_INPUT_LEN + 1], tempSubject[MAX_INPUT_LEN + 1],
	tempAttachments[MAX_INPUT_LEN + 1], tempText[MAX_INPUT_LEN + 1];

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
			return (res);
		}

		res = send_compose_message_from_mail(mainSocket, &mail, chatSocket, recv_chat_message_and_print);
		res = handle_return_value(res);
		if (res == ERROR) {
			free_mail(&mail);
			return (res);
		}
		free_mail(&mail);

		res = recv_send_result(mainSocket, chatSocket, recv_chat_message_and_print);
		res = handle_return_value(res);
		if (res == ERROR) {
			return (res);
		} else {
			printf(MAIL_SENT_MESSAGE);
		}
	}


	return (0);
}

int do_chat(int mainSocket, int chatSocket, char *toChat, char *textChat, char *userName){
	int res;
	Mail mail;

	res = prepare_mail_from_compose_input(&mail, userName, toChat,
			NULL, NULL, textChat);
	res = handle_return_value(res);
	if (res == ERROR) {
		return (res);
	}

	res = send_compose_message_from_mail(mainSocket, &mail, chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		free_mail(&mail);
		return (res);
	}
	free_mail(&mail);

	res = recv_send_result(mainSocket, chatSocket, recv_chat_message_and_print);
	res = handle_return_value(res);
	if (res == ERROR) {
		return (res);
	} else {
		printf(MAIL_SENT_MESSAGE);
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
	char userName[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	char input[MAX_INPUT_LEN + 1];
	char *stringMessage;
	char attachmentPath[MAX_PATH_LEN + 1];
	int isLoggedIn = 0;
	unsigned short mailID, tempAttachmentID;
	fd_set readfds, errorfds;
	int maxFd;
	char toChat[MAX_INPUT_LEN + 1], textChat[MAX_INPUT_LEN + 1];


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
	res = recv_string_from_message(clientSocket, &stringMessage, -1, NULL);
	res = handle_return_value(res);
	if (res == ERROR) {
		close(clientSocket);
		freeaddrinfo(servinfo);
		return (ERROR);
	} else {
		printf("%s\n", stringMessage);
		free(stringMessage);
	}

	res = recv_string_from_message(chatSocket, &stringMessage, -1, NULL);
	res = handle_return_value(res);
	if (res == ERROR) {
		close(chatSocket);
		freeaddrinfo(servinfo);
		return (ERROR);
	} else {
		free(stringMessage);
	}

	init_FD_sets(&readfds, NULL, &errorfds);
	FD_SET(STDIN_FILENO, &readfds);
	FD_SET(STDIN_FILENO, &errorfds);
	maxFd = STDIN_FILENO;

	do {
		res = 0;
		select(maxFd, &readfds, NULL, &errorfds, NULL);
		if ((FD_ISSET(STDIN_FILENO, &errorfds)) || (isLoggedIn  && FD_ISSET(chatSocket, &errorfds))){
			/* TODO: check error */
			break;
		}
		if (isLoggedIn && (FD_ISSET(chatSocket, &readfds))){
			do_get_chat(chatSocket);
		}

		if (FD_ISSET(STDIN_FILENO, &readfds)){
			fgets(input, MAX_INPUT_LEN, stdin);

			if (strcmp(input, QUIT_MESSAGE) == 0) {
				do_quit(clientSocket, chatSocket);
			} else if (!isLoggedIn) {
				res = do_credentials(clientSocket, chatSocket, &isLoggedIn, userName, password,  readfds, errorfds, &maxFd);
				res = handle_return_value(res);
				/* Flushing */
				fgets(input, MAX_INPUT_LEN, stdin);
			} else if (strcmp(input, SHOW_INBOX) == 0) {
				res = do_show_inbox(clientSocket, chatSocket);
			} else if (sscanf(input, GET_MAIL "%hu", &mailID) == 1) {
				res = do_get_mail(clientSocket, chatSocket, mailID);
			} else if (sscanf(input, GET_ATTACHMENT "%hu %hu \"%[^\"]\"", &mailID,
					&tempAttachmentID, attachmentPath) == 3) {
				res = do_get_attachment(clientSocket, chatSocket, (unsigned char)tempAttachmentID, attachmentPath, mailID);
			} else if (sscanf(input, DELETE_MAIL "%hu", &mailID) == 1) {
				res = do_delete_mail(clientSocket, chatSocket, mailID);
			} else if (strcmp(input, COMPOSE) == 0) {
				res = do_compose(clientSocket, chatSocket, userName);
			} else if (sscanf(input, "MSG: %[^\n]: %[^\n]\n", toChat, textChat) == 2) {
				res = do_chat(clientSocket, chatSocket, toChat, textChat, userName);
			} else {
				print_error_message(INVALID_COMMAND_MESSAGE);
			}

			if (res == ERROR) {
				break;
			}
		}
	} while (1);

		/* Close connection and socket */
		close(clientSocket);
		freeaddrinfo(servinfo);

		return (res);
	}

