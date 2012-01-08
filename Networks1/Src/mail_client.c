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
#define CHAT_MESSAGE_FORMAT "New message from %s: %s\n"
#define ONLINE_USERS "Online users: "

/* Commands definitions */
#define QUIT_MESSAGE "QUIT\n"
#define SHOW_INBOX "SHOW_INBOX\n"
#define GET_MAIL "GET_MAIL "
#define GET_ATTACHMENT "GET_ATTACHMENT "
#define DELETE_MAIL "DELETE_MAIL "
#define COMPOSE "COMPOSE\n"
#define CHAT_MESSAGE "MSG "
#define SHOW_ONLINE_USERS "SHOW_ONLINE_USERS\n"

/* General Messages */
#define CONNECTION_SUCCEED_MESSAGE "Connected to server\n"
#define ATTACHMENT_SAVE_MESSAGE "‫‪Attachment saved‬‬\n"
#define MAIL_SENT_MESSAGE "Mail sent\n"
#define CHAT_MAIL_SENT_MESSAGE "User is offline, message sent as mail\n"

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

	memset(mail, 0, sizeof(Mail));

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

int recv_chat_message_and_print(int socket) {
	Mail mail;
	int res;

	res = recv_chat_from_message(socket, &mail);
	if (res != 0) {
		return (res);
	}

	printf(CHAT_MESSAGE_FORMAT, mail.sender, mail.body);
	free_mail(&mail);
	return (res);
}

int do_quit(int mainSocket, int chatSocket){
	int res;

	res = send_quit_message(mainSocket);
	res = handle_return_value(res);

	res = send_quit_message(chatSocket);
	res = handle_return_value(res);

	return (res);
}

int do_credentials(int mainSocket, int chatSocket, int *isLoggedIn, char *userName, char *password, char *input) {
	int res = 0;

	if ((sscanf(input, "User: %s", userName) + scanf("Password: %s", password)) != 2) {
		print_error_message(CREDENTIALS_USAGE_MESSAGE);
	} else {
		res = send_message_from_credentials(mainSocket, chatSocket, userName, password);
		if (res != 0) {
			return (res);
		}

		res = recv_credentials_result(mainSocket, isLoggedIn, -1, NULL);
		if (res != 0) {
			return (res);
		} else {
			if (*isLoggedIn) {
				printf(CONNECTION_SUCCEED_MESSAGE);
			} else {
				print_error_message(WRONG_CREDENTIALS_MESSAGE);
			}
		}
	}

	return (res);
}

int do_show_inbox(int mainSocket, int chatSocket){
	int res;
	unsigned short mailAmount;
	Mail *mails;

	res = send_show_inbox_message(mainSocket, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		return (res);
	}

	mailAmount = -1;
	mails = NULL;
	res = recv_inbox_content_from_message(mainSocket, &mails, &mailAmount,
			chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		if (mails != NULL) {
			free_mails(mailAmount, mails);
			free(mails);
		}
		return (res);
	}

	print_inbox_info(mailAmount, mails);
	free_mails(mailAmount, mails);
	free(mails);

	return (res);
}

int do_get_mail(int mainSocket, int chatSocket, unsigned short mailID){
	int res;
	Mail mail;

	res = send_get_mail_message(mainSocket, mailID, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		return (res);
	}

	memset(&mail, 0, sizeof(Mail));
	res = recv_mail_from_message(mainSocket, &mail, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		free_mail(&mail);
		return (res);
	} else if (res == ERROR_INVALID_ID) {
		free_mail(&mail);
		return (res);
	} else {
		print_mail(&mail);
		free_mail(&mail);
	}

	return (res);
}

int do_get_attachment(int mainSocket, int chatSocket, unsigned char attachmentID,
		char * attachmentPath, unsigned short mailID){
	int res;
	Attachment attachment;

	memset(&attachment, 0, sizeof(Attachment));

	/* Converting to unsigned char while zeroing the non important bits */
	res = send_get_attachment_message(mainSocket, mailID, attachmentID, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		return (res);
	}

	res = recv_attachment_file_from_message(mainSocket, &attachment, attachmentPath,
			chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		free_attachment(&attachment);
		return (res);
	} else {
		printf(ATTACHMENT_SAVE_MESSAGE);
	}

	return (res);
}

int do_delete_mail(int mainSocket, int chatSocket, unsigned short mailID){
	int res;

	res = send_delete_mail_message(mainSocket, mailID, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		return (res);
	}

	res = recv_delete_result(mainSocket, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		return (res);
	}

	return (res);
}

int do_compose(int mainSocket, int chatSocket, char *userName){
	int res;
	Mail mail;
	char input[MAX_INPUT_LEN + 1];
	char tempRecipients[MAX_INPUT_LEN + 1], tempSubject[MAX_INPUT_LEN + 1],
	tempAttachments[MAX_INPUT_LEN + 1], tempText[MAX_INPUT_LEN + 1];

	/* Checking input */
	res = 1;
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
					res = 0;
				}
			}
		}
	}

	if (res == 1) {
		print_error_message(COMPOSE_USAGE_MESSAGE);
		res = 0;
	} else {
		res = prepare_mail_from_compose_input(&mail, userName, tempRecipients,
				tempSubject, tempAttachments, tempText);
		if (res != 0) {
			free_mail(&mail);
			return (res);
		}

		res = send_compose_message_from_mail(mainSocket, &mail, chatSocket, recv_chat_message_and_print);
		if (res != 0) {
			free_mail(&mail);
			return (res);
		}
		free_mail(&mail);

		res = recv_send_result(mainSocket, chatSocket, recv_chat_message_and_print);
		if (res != 0) {
			return (res);
		} else {
			printf(MAIL_SENT_MESSAGE);
		}
	}


	return (res);
}

int do_chat(int mainSocket, int chatSocket, char *toChat, char *textChat, char *userName){
	int res;
	Mail mail;
	int isMailSent;

	res = prepare_mail_from_compose_input(&mail, userName, toChat,
			"", "", textChat);
	if (res != 0) {
		free_mail(&mail);
		return (res);
	}

	res = send_chat_from_mail(mainSocket, &mail, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		free_mail(&mail);
		return (res);
	}
	free_mail(&mail);

	res = recv_chat_result(mainSocket, &isMailSent, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		return (res);
	} else if (isMailSent) {
		printf(CHAT_MAIL_SENT_MESSAGE);
	}

	return (res);
}

void print_online_users(char **onlineUsersNames, int usersAmount) {
	int i;

	printf(ONLINE_USERS);
	for (i = 0; i < usersAmount; i++) {
		printf((i+1 == usersAmount ? "%s\n" : "%s,"), onlineUsersNames[i]);
	}
}

int do_show_online_users(int clientSocket, int chatSocket) {
	int res;
	int usersAmount;
	char **onlineUsersNames = NULL;

	res = send_show_online_users(clientSocket, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		return (res);
	}

	res = recv_online_users(clientSocket, &onlineUsersNames, &usersAmount, chatSocket, recv_chat_message_and_print);
	if (res != 0) {
		if (onlineUsersNames != NULL) {
			free_online_users_names(onlineUsersNames);
			free(onlineUsersNames);
		}
		return (res);
	}

	print_online_users(onlineUsersNames, usersAmount);
	free_online_users_names(onlineUsersNames);
	free(onlineUsersNames);

	return (res);
}

void refresh_sets(fd_set *readfds, fd_set *errorfds, int isLoggedIn, int *maxFd, int socket){
	FD_ZERO(readfds);
	FD_ZERO(errorfds);
	FD_SET(STDIN_FILENO, readfds);
	FD_SET(STDIN_FILENO, errorfds);
	*maxFd = STDIN_FILENO + 1;
	if (isLoggedIn){
		FD_SET(socket, readfds);
		FD_SET(socket, errorfds);
		*maxFd = socket + 1;
	}
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
	memset(&hints, 0, sizeof(hints));
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
		close(clientSocket);
		close(chatSocket);
		freeaddrinfo(servinfo);
		return (ERROR);
	} else {
		free(stringMessage);
	}

	do {
		refresh_sets(&readfds, &errorfds, isLoggedIn, &maxFd, chatSocket);

		res = select(maxFd, &readfds, NULL, &errorfds, NULL);
		res = handle_return_value(res);
		if (res == ERROR) {
			break;
		}

		if ((FD_ISSET(STDIN_FILENO, &errorfds)) || (isLoggedIn && FD_ISSET(chatSocket, &errorfds))) {
			/* TODO: check error */
			break;
		}

		if (isLoggedIn && (FD_ISSET(chatSocket, &readfds))) {
			recv_chat_message_and_print(chatSocket);
		}

		if (FD_ISSET(STDIN_FILENO, &readfds)){
			fgets(input, MAX_INPUT_LEN, stdin);

			if (strcmp(input, QUIT_MESSAGE) == 0) {
				res = do_quit(clientSocket, chatSocket);
				break;
			} else if (!isLoggedIn) {
				res = do_credentials(clientSocket, chatSocket, &isLoggedIn, userName, password, input);
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
			} else if (sscanf(input, CHAT_MESSAGE "%[^:]: %[^\n]\n", toChat, textChat) == 2) {
				res = do_chat(clientSocket, chatSocket, toChat, textChat, userName);
			} else if (strcmp(input, SHOW_ONLINE_USERS) == 0) {
				res = do_show_online_users(clientSocket, chatSocket);
			} else {
				print_error_message(INVALID_COMMAND_MESSAGE);
			}

			res = handle_return_value(res);
			if (res == ERROR) {
				break;
			}
		}
	} while (1);

	/* Close connection and socket */
	close(clientSocket);
	close(chatSocket);
	freeaddrinfo(servinfo);

	return (res);
}

