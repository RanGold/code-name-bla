#define WELLCOME_MESSAGE "Welcome! I am simple-mail-server."
#define SERVER_USAGE_MSG "Usage mail_server <users_file> [port]"
#define INIT_USER_ARR_FAILED "Failed initiallizing users array"
#define DEAFULT_PORT 6423

#include "common.h"
#include "protocol.h"

typedef struct {
	Message message;
	int offset;
} UserBuffer;

typedef struct {
	char name[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	unsigned short mailsUsed;
	unsigned short mailArraySize;
	Mail** mails;
	int mainSocket;
	int chatSocket;
	int isOnline;
	UserBuffer mainBuffer;
	UserBuffer chatBuffer;
} User;

typedef struct {
	int socket;
	UserBuffer buffer;
	int isActive;
} UnrecognizedUser;

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

	int i, j, k, l;

	if (users != NULL){
		for (i = 0; i < usersAmount; i++) {
			for (j = 0; j < users[i].mailsUsed; j++) {
				if (users[i].mails[j] != NULL) {
					/* Nullifying the mail in all of its occurrences */
					for (k = i + 1; (k < usersAmount) &&
					(users[i].mails[j]->numRefrences > 1); k++) {
						for (l = 0; (l < users[k].mailsUsed) &&
						(users[i].mails[j]->numRefrences > 1); l++) {
							if (users[k].mails[l] == users[i].mails[j]) {
								users[i].mails[j]->numRefrences--;
								users[k].mails[l] = NULL;
							}
						}
					}

					free_mail(users[i].mails[j]);
					free(users[i].mails[j]);
				}
			}

			if (users[i].mails != NULL){
				free(users[i].mails);
				users[i].mails = NULL;
			}

			free_message(&(users[i].mainBuffer.message));
			free_message(&(users[i].chatBuffer.message));
		}

		free(users);
		users = NULL;
	}
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

void prepare_client_ids(User *user) {

	unsigned short i;

	for (i = 0; i < user->mailsUsed; i++) {
		if (user->mails[i] != NULL) {
			user->mails[i]->clientId = i + 1;
		}
	}
}

Mail* get_mail_by_id (User *user, unsigned short mailID) {

	if ((mailID <= 0) || (mailID > user->mailsUsed)) {
		return (NULL);
	} else {
		return (user->mails[mailID - 1]);
	}

	return (NULL);
}

Attachment* get_attachment_by_id (User *user, short mailID, unsigned char attachmentID) {

	Mail* mail = get_mail_by_id(user, mailID);

	if ((mail != NULL) && (attachmentID > 0) && (mail->numAttachments >= attachmentID)) {
		return (mail->attachments + attachmentID - 1);
	}

	return (NULL);
}

int delete_mail(User *user, unsigned short mailID) {

	Mail *mail = get_mail_by_id(user, mailID);

	if (mail == NULL) {
		return (ERROR_INVALID_ID);
	}

	/* Removing reference */
	mail->numRefrences--;

	/* Freeing the mail struct if needed */
	if (mail->numRefrences == 0) {
		free_mail(mail);
		free(user->mails[mailID - 1]);
	}

	user->mails[mailID - 1] = NULL;

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

	int i, j, k;

	/* Setting sender and freeing the current empty sender */
	free(mail->sender);
	mail->sender = calloc(strlen(curUserName) + 1, 1);
	if (mail->sender == NULL) {
		return (ERROR);
	}
	strncpy(mail->sender, curUserName, strlen(curUserName));

	/* Adding mail to recipients */
	/* Ignoring non exiting recipients */
	mail->numRefrences = 0;
	for (i = 0; i < usersAmount; i++) {
		for (j = 0; j < mail->numRecipients; j++) {
			if (strcmp(mail->recipients[j], users[i].name) == 0) {
				/* Checking if the mail array should be enlarged */
				if (users[i].mailsUsed == users[i].mailArraySize) {
					users[i].mailArraySize *= 2;
					users[i].mails = realloc(users[i].mails, users[i].mailArraySize * sizeof(Mail*));
					if (users[i].mails == NULL) {
						return(ERROR);
					}
					/* Initializing new mails */
					for (k = users[i].mailArraySize / 2; k < users[i].mailArraySize; k++) {
						users[i].mails[k] = NULL;
					}
				}

				users[i].mails[users[i].mailsUsed] = mail;
				users[i].mailsUsed++;
				mail->numRefrences++;
			}
		}
	}

	return (0);
}

int add_unrecognized_socket(UnrecognizedUser **unrecognizedUsers, int *unrecognizedUsersSize,
		int *unreconizedUsersAmount, int unrecognizedSocket, Message message){
	int i;

	for (i = 0; i < *unreconizedUsersAmount; i++){
		if (!(*unrecognizedUsers)[i].isActive){
			(*unrecognizedUsers)[i].buffer.message = message;
			(*unrecognizedUsers)[i].isActive = 1;
			(*unrecognizedUsers)[i].socket = unrecognizedSocket;
			(*unreconizedUsersAmount)++;
			break;
		}
	}

	if (*unrecognizedUsersSize == *unreconizedUsersAmount){
		(*unrecognizedUsersSize) *= 2;
		*unrecognizedUsers = realloc(*unrecognizedUsers, *unrecognizedUsersSize * sizeof(UnrecognizedUser));
		if (*unrecognizedUsers == NULL){
			return (ERROR);
		}
	}
	return (0);

}

void handleErrorFds(fd_set *errorfds, User *users, UnrecognizedUser *unrecognizedUsers, int usersAmount,
					int unrecognizedUsersAmount) {
	int i;

	for (i = 0; i < usersAmount; i++){
		if (FD_ISSET(users[i].mainSocket, errorfds) ){
			users[i].isOnline = 0;
			close(users[i].mainSocket);
			close(users[i].chatSocket);
			users[i].mainSocket = -1;
			users[i].chatSocket = -1;
		}

		free_message(&(users[i].mainBuffer.message));
		free_message(&(users[i].chatBuffer.message));
	}

	for (i = 0; i < unrecognizedUsersAmount; i++){
		if (FD_ISSET(unrecognizedUsers[i].socket, errorfds) ){
			close(unrecognizedUsers[i].socket);
			unrecognizedUsers[i].socket = -1;
			unrecognizedUsers[i].isActive = 0;
			free_message(&(unrecognizedUsers[i].buffer.message));
		}
	}
}

int handleReadFds(fd_set *readfds, int listenSocket, User *users, int userAmounts,
		UnrecognizedUser **unrecognizedUsers, int *unreconizedUsersAmount, int *unrecognizedUsersSize) {
	int i, unrecognizedSocket, res;
	unsigned int len;
	struct sockaddr_in clientAddr;
	Message message;

	if (FD_ISSET(listenSocket, readfds)){
		/* Prepare structure for client address */
		len = sizeof(clientAddr);

		/* Start waiting until client connect */
		unrecognizedSocket = accept(listenSocket, (struct sockaddr*) &clientAddr, &len);
		if (unrecognizedSocket == -1) {
			print_error();
			return ERROR;
		}

		res = prepare_message_from_string(WELLCOME_MESSAGE, &message);
		if (res != 0) {
			free_message(&message);
			return (res);
		}

		res = add_unrecognized_socket(unrecognizedUsers, unrecognizedUsersSize,
								unreconizedUsersAmount, unrecognizedSocket, message);
		if (res == ERROR){
			free_message(&message);
			return (res);
		}
	}


	for (i = 0; i < userAmounts; i++){
		if ((users[i].isOnline) && (FD_ISSET(users[i].mainSocket, readfds))){

		}
	}
}

void refreshSets(fd_set *readfds, fd_set *writefds, fd_set *errorfds, int *maxSocket,
				int listenSocket, User *users, int userAmount,
				UnrecognizedUser *unrecognizedUsers, int unrecognizedUsersAmount) {
	int i;
	*maxSocket = listenSocket;

	FD_ZERO(readfds);
	FD_ZERO(writefds);
	FD_ZERO(errorfds);

	FD_SET(listenSocket, readfds);

	for (i = 0; i < userAmount; i++) {
		if (users[i].isOnline) {
			if (users[i].mainSocket > *maxSocket){
				*maxSocket = users[i].mainSocket;
			}
			if (users[i].chatSocket > *maxSocket){
				*maxSocket = users[i].chatSocket;
			}
			FD_SET(users[i].mainSocket, readfds);
			FD_SET(users[i].mainSocket, writefds);
			FD_SET(users[i].chatSocket, writefds);
			FD_SET(users[i].mainSocket, errorfds);
			FD_SET(users[i].chatSocket, errorfds);
		}
	}
	for (i = 0; i < unrecognizedUsersAmount; i++) {
		if (unrecognizedUsers[i].isActive){
			if (unrecognizedUsers[i].socket > *maxSocket){
					*maxSocket = unrecognizedUsers[i].socket;
				}
				FD_SET(unrecognizedUsers[i].socket, readfds);
				FD_SET(unrecognizedUsers[i].socket, writefds);
				FD_SET(unrecognizedUsers[i].socket, errorfds);
			}
		}

	(*maxSocket)++;
}



int main(int argc, char** argv) {

	/* Variables declaration */
	short port = DEAFULT_PORT;
	int usersAmount, res;
	User *users = NULL, *curUser = NULL;
	int listenSocket, clientSocket;
	unsigned short mailID;
	unsigned char attachmentID;
	Message message;
	Mail *mail;
	Attachment *attachment;
	fd_set readfds, writefds, errorfds;
	int maxfd;
	struct timeval tv;
	UnrecognizedUser *unrecognizedUsers;
	int unrecognizedUsersSize;
	int unrecognizedUsersInUse;

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

	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	unrecognizedUsers = (UnrecognizedUser*)calloc(1, sizeof(UnrecognizedUser));
	if (unrecognizedUsers == NULL) {
		/* TODO : free unrecognized users */
		print_error();
		return (ERROR);
	}
	memset(&(unrecognizedUsers[0].buffer), 0, sizeof(UserBuffer));
	unrecognizedUsersInUse = 0;
	unrecognizedUsersSize = 1;


	do {
		refreshSets(&readfds, &writefds, &errorfds, &maxfd, listenSocket, users, usersAmount,
					unrecognizedUsers, unrecognizedUsersSize);
		res = select(maxfd, &readfds, &writefds, &errorfds, &tv);
		res = handle_return_value(res);

		if (res == ERROR) {
			break;
		}
		handleErrorFds(&errorfds, users, unrecognizedUsers, usersAmount, unrecognizedUsersSize);
		handleReadFds(&readfds, listenSocket, users, usersAmount, &unrecognizedUsers, &unrecognizedUsersInUse,
					&unrecognizedUsersSize);
		handleSendFds(&writefds, users);

	} while (1);



	do {




		/* Sending welcome message */
		res = send_message_from_string(clientSocket, WELLCOME_MESSAGE);
		res = handle_return_value(res);
		if (res == 0) {
			memset(&message, 0, sizeof(Message));
			do {
				/* Waiting for client request */
				res = recv_message(clientSocket, &message);
				res = handle_return_value(res);
				if (res == ERROR) {
					break;
				}

				if (message.messageType == Quit) {
					break;
				} else if (curUser == NULL) {
					curUser = check_credentials_message(users, usersAmount,
							&message);

					if (curUser == NULL) {
						res = send_credentials_deny_message(clientSocket);
					} else {
						res = send_credentials_approve_message(clientSocket);
					}

					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}
				} else if (message.messageType == ShowInbox) {
					prepare_client_ids(curUser);
					res = send_message_from_inbox_content(clientSocket,
							curUser->mails, curUser->mailsUsed);
					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}
				} else if (message.messageType == GetMail) {

					prepare_mail_id_from_message(&message, &mailID, GetMail);
					if (mailID == ERROR_LOGICAL) {
						print_error_message(INVALID_DATA_MESSAGE);
						break;
					}

					mail = get_mail_by_id(curUser, mailID);
					if (mail == NULL) {
						res = send_invalid_id_message(clientSocket);
					} else {
						res = send_message_from_mail(clientSocket, mail);
					}

					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}
				} else if (message.messageType == GetAttachment) {

					prepare_mail_attachment_id_from_message(&message, &mailID,
							&attachmentID);
					if (mailID == ERROR_LOGICAL) {
						print_error_message(INVALID_DATA_MESSAGE);
						break;
					}

					attachment = get_attachment_by_id(curUser, mailID,
							attachmentID);
					if (attachment == NULL) {
						res = send_invalid_id_message(clientSocket);
					} else {
						res = send_message_from_attachment(clientSocket,
								attachment);
					}

					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}
				} else if (message.messageType == DeleteMail) {
					prepare_mail_id_from_message(&message, &mailID, DeleteMail);
					if (mailID == ERROR_LOGICAL) {
						print_error_message(INVALID_DATA_MESSAGE);
						break;
					}

					res = delete_mail(curUser, mailID);
					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}
					if (res == ERROR_INVALID_ID) {
						res = send_invalid_id_message(clientSocket);
					} else {
						res = send_delete_approve_message(clientSocket);
					}

					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}
				} else if (message.messageType == Compose) {
					res = prepare_mail_from_compose_message(&message, &mail);
					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}

					res = add_mail_to_server(users, usersAmount, curUser->name,
							mail);
					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}

					res = send_send_approve_message(clientSocket);
					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}
				} else {
					res = send_invalid_command_message(clientSocket);
					res = handle_return_value(res);
					if (res == ERROR) {
						break;
					}
				}

				free_message(&message);
			} while (1);
		}

		curUser = NULL;
		free_message(&message);
		close(clientSocket);
	} while (1);

	/* Releasing resources */
	free_users_array(users, usersAmount);
	close(listenSocket);
	free_message(&message);

	return (res);
}
