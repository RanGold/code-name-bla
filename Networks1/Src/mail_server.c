#define WELLCOME_MESSAGE "Welcome! I am simple-mail-server."
#define SERVER_USAGE_MSG "Usage mail_server <users_file> [port]"
#define INIT_USER_ARR_FAILED "Failed initiallizing users array"
#define CHAT_MESSAGE_SUBJECT "Message received while offline"
#define DEAFULT_PORT 6423
#define SELECT_UTIMEVAL 100000
#define LISTEN_QUEUE_SIZE 20

#include "common.h"
#include "protocol.h"

typedef struct {
	char name[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	unsigned short mailsUsed;
	unsigned short mailArraySize;
	Mail** mails;
	int isOnline;
	int mainSocket;
	int chatSocket;
	NonBlockingMessage mainBuffer;
	NonBlockingMessage chatBuffer;
} User;

typedef struct {
	int socket;
	NonBlockingMessage buffer;
	int isActive;
} UnrecognizedUser;

struct ChatQueue {
	NonBlockingMessage chatBuffer;
	User* fromUser;
	User* toUser;
	struct ChatQueue *next;
};

typedef struct ChatQueue ChatQueue;

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

			if (users[i].mails != NULL) {
				free(users[i].mails);
				users[i].mails = NULL;
			}

			if (users[i].isOnline) {
				close(users[i].mainSocket);
				close(users[i].chatSocket);
			}

			free_non_blocking_message(&(users[i].mainBuffer));
			free_non_blocking_message(&(users[i].chatBuffer));
		}

		free(users);
		users = NULL;
	}
}

int initialize_users_array(int* usersAmount, User** users, char* filePath) {

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

		(*users)[i].mainSocket = -1;
		(*users)[i].chatSocket = -1;
		(*users)[i].isOnline = 0;
	}

	fclose(usersFile);
	return (0);
}

void free_unrecognized_users_array(UnrecognizedUser *unrecognizedUsers, int unrecognizedUsersSize) {
	int i;

	if (unrecognizedUsers != NULL) {
		for (i = 0; i < unrecognizedUsersSize; i++) {
			free_non_blocking_message(&(unrecognizedUsers[i].buffer));

			if (unrecognizedUsers[i].isActive) {
				close(unrecognizedUsers[i].socket);
			}
		}

		free(unrecognizedUsers);
	}
}

int initialize_unrecognized_users_array(UnrecognizedUser **unrecognizedUsers) {
	(*unrecognizedUsers) = (UnrecognizedUser*)calloc(1, sizeof(UnrecognizedUser));
	if ((*unrecognizedUsers) == NULL) {
		return (ERROR);
	}

	(*unrecognizedUsers)[0].socket = -1;

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
	res = listen(*listenSocket, LISTEN_QUEUE_SIZE);
	if (res == -1) {
		close(*listenSocket);
		return (ERROR);
	}

	return (0);
}

User* get_user_by_name(User *users, int usersAmount, char *name) {
	int i;

	for (i = 0; i < usersAmount; i++) {
		if (strcmp(users[i].name, name) == 0) {
			return (users +i);
		}
	}

	return (NULL);
}

int set_mail_sender(char* curUserName, Mail *mail) {
	free(mail->sender);
	mail->sender = calloc(strlen(curUserName) + 1, 1);
	if (mail->sender == NULL) {
		return (ERROR);
	}
	strncpy(mail->sender, curUserName, strlen(curUserName));

	return (0);
}

int add_mail_to_server(User *users, int usersAmount, char *curUserName, Mail *mail, int isChatMessage) {

	int i, j, k;

	/* Setting sender and freeing the current empty sender */
	if (set_mail_sender(curUserName, mail) != 0) {
		return (ERROR);
	}

	/* If chat message setting subject and freeing the current empty subject */
	if (isChatMessage) {
		free(mail->subject);
		mail->subject = calloc(strlen(CHAT_MESSAGE_SUBJECT) + 1, 1);
		if (mail->subject == NULL) {
			return (ERROR);
		}
		strncpy(mail->subject, CHAT_MESSAGE_SUBJECT, strlen(CHAT_MESSAGE_SUBJECT));
	}

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

UnrecognizedUser* add_unrecognized_socket(UnrecognizedUser **unrecognizedUsers, int *unrecognizedUsersSize,
		int *unreconizedUsersAmount, int unrecognizedSocket) {
	int i;
	UnrecognizedUser *newUser;

	/* Checking if the unrecognized users array needs to be resized */
	if (*unrecognizedUsersSize == *unreconizedUsersAmount){
		(*unrecognizedUsersSize) *= 2;
		(*unrecognizedUsers) = realloc(*unrecognizedUsers, *unrecognizedUsersSize * sizeof(UnrecognizedUser));
		if (*unrecognizedUsers == NULL){
			return (NULL);
		}

		/* Initializing new unrecognized users */
		for (i = (*unrecognizedUsersSize) / 2; i < (*unrecognizedUsersSize); i++) {
			memset(&((*unrecognizedUsers)[i].buffer), 0, sizeof(NonBlockingMessage));
			(*unrecognizedUsers)[i].socket = -1;
			(*unrecognizedUsers)[i].isActive = 0;
		}
	}

	/* Searching for a free unrecognized user spot */
	for (i = 0; i < (*unrecognizedUsersSize); i++) {
		if (!(*unrecognizedUsers)[i].isActive) {
			(*unrecognizedUsers)[i].isActive = 1;
			(*unrecognizedUsers)[i].socket = unrecognizedSocket;
			(*unreconizedUsersAmount)++;
			newUser = (*unrecognizedUsers) + i;
			break;
		}
	}

	return (newUser);
}

void remove_unrecognized_user(UnrecognizedUser *unrecognizedUsers) {
	unrecognizedUsers->isActive = 0;
	unrecognizedUsers->socket = -1;
	free_non_blocking_message(&(unrecognizedUsers->buffer));
}

void disconnect_unrecognized_user(UnrecognizedUser *unrecognizedUsers) {
	close(unrecognizedUsers->socket);
	remove_unrecognized_user(unrecognizedUsers);
}

/* Accepting new connections */
int do_accept(int listenSocket, UnrecognizedUser **unrecognizedUsers, int *unrecognizedUsersSize,
		int *unreconizedUsersAmount) {
	unsigned int len;
	struct sockaddr_in clientAddr;
	int res, unrecognizedSocket;
	UnrecognizedUser *newUser;

	/* Prepare structure for client address */
	len = sizeof(clientAddr);

	/* Start waiting until client connect */
	unrecognizedSocket = accept(listenSocket, (struct sockaddr*) &clientAddr, &len);
	if (unrecognizedSocket == -1) {
		print_error();
		return ERROR;
	}

	newUser = add_unrecognized_socket(unrecognizedUsers, unrecognizedUsersSize,
			unreconizedUsersAmount, unrecognizedSocket);
	if (newUser == NULL) {
		return (ERROR);
	}

	res = prepare_message_from_string(WELLCOME_MESSAGE, &(newUser->buffer));
	if (res != 0) {
		free_non_blocking_message(&(newUser->buffer));
		return (res);
	}

	return (0);
}

/* Prepares a message with the user's inbox info for the send phase */
int do_show_inbox(User* user) {
	int res;

	prepare_client_ids(user);
	free_non_blocking_message(&(user->mainBuffer));
	res = prepare_message_from_inbox_content(user->mails, user->mailsUsed,
			&(user->mainBuffer));

	return res;
}

/* Prepares the buffer with the mail for the send phase */
int do_get_mail(User *curUser) {
	int res = 0;
	Mail *mail;
	unsigned short mailID;

	mailID = prepare_mail_id_from_message(&(curUser->mainBuffer), GetMail);
	if (mailID == ERROR_LOGICAL) {
		return (mailID);
	}

	mail = get_mail_by_id(curUser, mailID);
	if (mail == NULL) {
		prepare_invalid_id_message(&(curUser->mainBuffer));
	} else {
		res = prepare_message_from_mail(mail, &(curUser->mainBuffer), 0);
	}

	return (res);
}

/* Prepares the buffer with the attachment for the send phase */
int do_get_attachment(User *curUser) {
	int res = 0;
	unsigned short mailID;
	unsigned char attachmentID;
	Attachment *attachment;

	prepare_mail_attachment_id_from_message(&(curUser->mainBuffer), &mailID, &attachmentID);
	if (mailID == ERROR_LOGICAL) {
		return (mailID);
	}

	attachment = get_attachment_by_id(curUser, mailID, attachmentID);
	if (attachment == NULL) {
		prepare_invalid_id_message(&(curUser->mainBuffer));
	} else {
		res = prepare_message_from_attachment(attachment, &(curUser->mainBuffer));
	}

	return (res);
}

/* Deletes the mail and prepares the buffer with the result of the deletion for the send phase */
int do_delete_mail(User *curUser) {
	int res = 0;
	unsigned short mailID;

	mailID = prepare_mail_id_from_message(&(curUser->mainBuffer), DeleteMail);
	if (mailID == ERROR_LOGICAL) {
		return (mailID);
	}

	res = delete_mail(curUser, mailID);
	if (res == ERROR) {
		return (res);
	} else if (res == ERROR_INVALID_ID) {
		prepare_invalid_id_message(&(curUser->mainBuffer));
		res = 0;
	} else {
		prepare_delete_approve_message(&(curUser->mainBuffer));
	}

	return (res);
}

/* Inserts the new mail message to the correct users arrays */
int do_compose(User *users, int usersAmount, User *curUser) {
	int res;
	Mail *mail;

	res = prepare_mail_from_compose_message(&(curUser->mainBuffer), &mail);
	if (res == ERROR) {
		return (res);
	}

	res = add_mail_to_server(users, usersAmount, curUser->name,
			mail, 0);
	if (res == ERROR) {
		return (res);
	}

	prepare_send_approve_message(&(curUser->mainBuffer));

	return (res);
}

void do_invalid_message(NonBlockingMessage *nbMessage) {
	free_non_blocking_message(nbMessage);
	prepare_invalid_command_message(nbMessage);
}

void release_chat_queue(ChatQueue *chatQueueHead) {
	ChatQueue *cur;

	while (chatQueueHead != NULL) {
		cur = chatQueueHead;
		chatQueueHead = cur->next;
		free(cur);
	}
}

/* Adds the chat message to the queue */
int add_chat_message_to_queue(Mail *chatMessage, ChatQueue **chatQueueHead, User *fromUser, User *toUser) {

	int res;
	ChatQueue *prev = NULL, *cur = *chatQueueHead;

	while ((cur) != NULL) {
		prev = cur;
		cur = cur->next;
	}

	/* Check for empty Queue */
	if (prev == NULL) {
		(*chatQueueHead) = calloc(1, sizeof(ChatQueue));
		if ((*chatQueueHead) == NULL) {
			free_mail(chatMessage);
			return (ERROR);
		}

		cur = *chatQueueHead;
	} else {
		prev->next = calloc(1, sizeof(ChatQueue));
		if (prev->next == NULL) {
			free_mail(chatMessage);
			return (ERROR);
		}

		cur = prev->next;
	}

	res = prepare_message_from_mail(chatMessage, &(cur->chatBuffer), 1);
	free_mail(chatMessage);
	cur->toUser = toUser;
	cur->fromUser = fromUser;

	return (res);
}

/* 1. Check if user is online and prepare chatmessage in chatQueue for send phase
   2. If not online - compose mail */
int do_chat_message_send(User *users, int usersAmount, User* curUser, ChatQueue **chatQueueHead) {
	int res;
	Mail *mail;
	User *toUser;

	/* This actually performs the same for any mail, and chat message is a mail */
	res = prepare_mail_from_compose_message(&(curUser->mainBuffer), &mail);
	if (res == ERROR) {
		free(mail);
		return (res);
	}

	if (mail->numRecipients != 1) {
		do_invalid_message(&(curUser->mainBuffer));
		free(mail);
		return (0);
	}

	toUser = get_user_by_name(users, usersAmount, mail->recipients[0]);
	if (toUser == NULL) {
		free_mail(mail);
	} else {
		if (!(toUser->isOnline)) {
			res = add_mail_to_server(users, usersAmount, curUser->name, mail, 1);
			if (res != 0) {
				return (res);
			}

			prepare_chat_mail_confirm_message(&(curUser->mainBuffer));
		} else {
			/* Setting mail's sender */
			res = set_mail_sender(curUser->name, mail);
			if (res != 0) {
				free_mail(mail);
				return (res);
			}

			res = add_chat_message_to_queue(mail, chatQueueHead, curUser, toUser);
			if (res != 0) {
				free_mail(mail);
				return (res);
			}

			free_mail(mail);
			prepare_chat_confirm_message(&(curUser->mainBuffer));
		}
	}

	return (res);
}

int prepare_chat_messages(User *users, int usersAmount, ChatQueue **chatQueueHead) {

	int res = 0;
	ChatQueue *prev = NULL, *cur = *chatQueueHead;
	Mail *mail;

	while ((cur) != NULL) {
		if (!(cur->toUser->isOnline)) {
			/* This actually performs the same for any mail, and chat message is a mail */
			res = prepare_mail_from_compose_message(&(cur->chatBuffer), &mail);
			if (res == ERROR) {
				return (res);
			}

			res = add_mail_to_server(users, usersAmount, cur->fromUser->name, mail, 1);
			if (res == ERROR) {
				return (res);
			}

			if (prev == NULL) {
				*chatQueueHead = cur->next;
				free(cur);
				cur = *chatQueueHead;
			} else {
				prev->next = cur->next;
				free(cur);
				cur = prev->next;
			}
		} else if (!is_there_message_to_send(&(cur->toUser->chatBuffer))) {
			/* Online and chat buffer is empty */
			cur->toUser->chatBuffer = cur->chatBuffer;

			if (prev == NULL) {
				*chatQueueHead = cur->next;
				free(cur);
				cur = *chatQueueHead;
			} else {
				prev->next = cur->next;
				free(cur);
				cur = prev->next;
			}
		} else {
			prev = cur;
			cur = cur->next;
		}
	}

	return (res);
}

int do_show_online_users(User *users, int usersAmount, User *curUser) {

	int i, onlineUsers, res;
	char **onlineUsersNames;

	onlineUsers = 0;
	for (i = 0; i < usersAmount; i++) {
		if (users[i].isOnline) {
			onlineUsers++;
		}
	}

	onlineUsersNames = calloc(onlineUsers, sizeof(char*));
	if (onlineUsersNames == NULL) {
		free_non_blocking_message(&(curUser->mainBuffer));
		return (ERROR);
	}

	onlineUsers = 0;
	for (i = 0; i < usersAmount; i++) {
		if (users[i].isOnline) {
			onlineUsersNames[onlineUsers] = users[i].name;
			onlineUsers++;
		}
	}

	free_non_blocking_message(&(curUser->mainBuffer));
	res = prepare_online_users_message(&(curUser->mainBuffer), onlineUsersNames, onlineUsers);
	free(onlineUsersNames);
	return (res);
}

/* Updates the unrecognized user status by its credential */
void do_handle_credentials(UnrecognizedUser *unrecognizedUser, User* users, int usersAmount) {
	User *curUser;
	MessageType messageType;

	messageType = unrecognizedUser->buffer.message.messageType;
	curUser = check_credentials_message(users, usersAmount,
			&(unrecognizedUser->buffer.message));

	if (curUser == NULL || curUser->isOnline) {
		free_non_blocking_message(&(unrecognizedUser->buffer));
		if (messageType == CredentialsMain) {
			prepare_credentials_deny_message(&(unrecognizedUser->buffer));
		}
	} else {

		if (messageType == CredentialsMain) {
			curUser->mainSocket = unrecognizedUser->socket;
			if (curUser->chatSocket != -1) {
				curUser->isOnline = 1;
				prepare_credentials_approve_message(&(curUser->mainBuffer));
			}
		} else {
			curUser->chatSocket = unrecognizedUser->socket;
			if (curUser->mainSocket != -1) {
				curUser->isOnline = 1;
				prepare_credentials_approve_message(&(curUser->mainBuffer));
			}
		}

		remove_unrecognized_user(unrecognizedUser);
	}
}

void remove_fd_from_fd_sets(int fd, fd_set *readfds, fd_set *writefds, fd_set *errorfds) {
	if (readfds != NULL && FD_ISSET(fd, readfds)) {
		FD_CLR(fd, readfds);
	}

	if (writefds != NULL && FD_ISSET(fd, writefds)) {
		FD_CLR(fd, writefds);
	}

	if (errorfds != NULL && FD_ISSET(fd, errorfds)) {
		FD_CLR(fd, errorfds);
	}
}

void disconnect_user(User *user) {
	close(user->chatSocket);
	close(user->mainSocket);
	user->isOnline = 0;
	user->mainSocket = -1;
	user->chatSocket = -1;
	free_non_blocking_message(&(user->mainBuffer));
	free_non_blocking_message(&(user->chatBuffer));
}

void remove_fd_from_fdsets(int fd, fd_set* readfds, fd_set* writefds, fd_set* errorfds) {
	if (readfds != NULL && FD_ISSET(fd, readfds)) {
		FD_CLR(fd, readfds);
	}

	if (writefds != NULL && FD_ISSET(fd, writefds)) {
		FD_CLR(fd, writefds);
	}

	if (errorfds != NULL && FD_ISSET(fd, errorfds)) {
		FD_CLR(fd, errorfds);
	}
}

void do_quit(User *user){
	disconnect_user(user);
}

void handle_error_fds(fd_set* readfds, fd_set* writefds, fd_set* errorfds, User *users, UnrecognizedUser *unrecognizedUsers,
		int usersAmount, int unrecognizedUsersAmount) {
	int i;

	for (i = 0; i < usersAmount; i++) {
		if (FD_ISSET(users[i].mainSocket, errorfds) || FD_ISSET(users[i].chatSocket, errorfds)) {
			remove_fd_from_fd_sets(users[i].mainSocket, readfds, writefds, NULL);
			remove_fd_from_fd_sets(users[i].chatSocket, readfds, writefds, NULL);
			disconnect_user(&(users[i]));
		}
	}

	for (i = 0; i < unrecognizedUsersAmount; i++) {
		if (FD_ISSET(unrecognizedUsers[i].socket, errorfds)) {
			close(unrecognizedUsers[i].socket);
			remove_fd_from_fd_sets(unrecognizedUsers[i].socket, readfds, writefds, NULL);
			unrecognizedUsers[i].socket = -1;
			unrecognizedUsers[i].isActive = 0;
			free_non_blocking_message(&(unrecognizedUsers[i].buffer));
		}
	}
}

/* TODO : because of read and write by stages there could be a situation where both are possible */
int handle_read_fds(fd_set* readfds, fd_set* writefds, int listenSocket, User *users, int usersAmount,
		UnrecognizedUser **unrecognizedUsers, int *unrecognizedUsersAmount, int *unrecognizedUsersSize,
		ChatQueue **chatQueueHead) {
	int i, res = 0;

	/* Check if listen socket was signaled */
	if (FD_ISSET(listenSocket, readfds)) {
		if (do_accept(listenSocket, unrecognizedUsers, unrecognizedUsersSize, unrecognizedUsersAmount) == ERROR) {
			return (ERROR);
		}
	}

	for (i = 0; i < usersAmount; i++) {
		if ((users[i].isOnline) && (FD_ISSET(users[i].mainSocket, readfds))) {
			res = recv_non_blocking_message(users[i].mainSocket, &(users[i].mainBuffer));
			if (res != 0) {
				remove_fd_from_fd_sets(users[i].mainSocket, readfds, writefds, NULL);
				remove_fd_from_fd_sets(users[i].chatSocket, readfds, writefds, NULL);
				disconnect_user(&(users[i]));
				continue;
			}

			if (is_full_message_received(&(users[i].mainBuffer))) {
				switch (users[i].mainBuffer.message.messageType) {
				case Quit:
					do_quit(&(users[i]));
					break;
				case ShowInbox:
					free_non_blocking_message(&(users[i].mainBuffer));
					res = do_show_inbox(users + i);
					break;
				case GetMail:
					res = do_get_mail(users + i);
					break;
				case GetAttachment:
					res = do_get_attachment(users + i);
					break;
				case DeleteMail:
					res = do_delete_mail(users + i);
					break;
				case Compose:
					res = do_compose(users, usersAmount, users + i);
					break;
				case ChatMessageSend:
					res = do_chat_message_send(users, usersAmount, users + i, chatQueueHead);
					break;
				case ShowOnlineUsers:
					res = do_show_online_users(users, usersAmount, users + i);
					break;
				default:
					do_invalid_message(&(users[i].mainBuffer));
				}
			}

			res = handle_return_value(res);
			if (res != 0) {
				remove_fd_from_fd_sets(users[i].mainSocket, readfds, writefds, NULL);
				remove_fd_from_fd_sets(users[i].chatSocket, readfds, writefds, NULL);
				disconnect_user(&(users[i]));
			}
		}
	}

	for (i = 0; i < (*unrecognizedUsersSize); i++) {
		if (((*unrecognizedUsers)[i].isActive) && (FD_ISSET((*unrecognizedUsers)[i].socket, readfds))) {
			res = recv_non_blocking_message((*unrecognizedUsers)[i].socket, &((*unrecognizedUsers)[i].buffer));
			if (res != 0) {
				remove_fd_from_fd_sets((*unrecognizedUsers)[i].socket, readfds, writefds, NULL);
				disconnect_unrecognized_user(*unrecognizedUsers + i);
			}

			if (is_full_message_received(&((*unrecognizedUsers)[i].buffer))) {
				switch ((*unrecognizedUsers)[i].buffer.message.messageType) {
				case CredentialsMain:
				case CredentialsChat:
					do_handle_credentials((*unrecognizedUsers) + i, users, usersAmount);
					break;
				case Quit:
					remove_fd_from_fd_sets((*unrecognizedUsers)[i].socket, readfds, writefds, NULL);
					disconnect_unrecognized_user(*unrecognizedUsers + i);
					break;
				default:
					do_invalid_message(&((*unrecognizedUsers)[i].buffer));
				}
			}
		}
	}

	return (0);
}

void handle_send_fds(fd_set *writefds, User *users, int userAmounts,
		UnrecognizedUser *unrecognizedUsers, int unreconizedUsersAmount) {
	int i, res;

	for (i = 0; i < userAmounts; i++){
		if (users[i].isOnline) {
			if (FD_ISSET(users[i].mainSocket, writefds)  && is_there_message_to_send(&(users[i].mainBuffer))) {
				res = send_non_blocking_message(users[i].mainSocket, &(users[i].mainBuffer));
				if (res != 0) {
					disconnect_user(users + i);
				}
			}

			if (FD_ISSET(users[i].chatSocket, writefds)  && is_there_message_to_send(&(users[i].chatBuffer))) {
				res = send_non_blocking_message(users[i].chatSocket, &(users[i].chatBuffer));
				if (res != 0) {
					disconnect_user(users + i);
				}
			}
		}
	}

	for (i = 0; i < unreconizedUsersAmount; i++) {
		if (unrecognizedUsers[i].isActive && FD_ISSET(unrecognizedUsers[i].socket, writefds) &&
				is_there_message_to_send(&(unrecognizedUsers[i].buffer))) {
			res = send_non_blocking_message(unrecognizedUsers[i].socket, &(unrecognizedUsers[i].buffer));
			if (res != 0) {
				disconnect_unrecognized_user(unrecognizedUsers + i);
			}
		}
	}
}

/* Preparing the file descriptors sets for the select */
void refresh_sets(fd_set *readfds, fd_set *writefds, fd_set *errorfds, int *maxSocket,
				int listenSocket, User *users, int userAmount,
				UnrecognizedUser *unrecognizedUsers, int unrecognizedUsersAmount) {
	int i;
	*maxSocket = listenSocket;

	init_FD_sets(readfds, writefds, errorfds);

	FD_SET(listenSocket, readfds);

	for (i = 0; i < userAmount; i++) {
		if (users[i].isOnline) {
			if (users[i].mainSocket > *maxSocket) {
				*maxSocket = users[i].mainSocket;
			}

			if (users[i].chatSocket > *maxSocket) {
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
		if (unrecognizedUsers[i].isActive) {
			if (unrecognizedUsers[i].socket > *maxSocket) {
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
	User *users = NULL;
	int listenSocket;
	fd_set readfds, writefds, errorfds;
	int maxfd;
	struct timeval tv;
	UnrecognizedUser *unrecognizedUsers;
	int unrecognizedUsersSize;
	int unrecognizedUsersInUse;
	ChatQueue *chatQueueHead = NULL;

	/* Validate number of arguments */
	if (argc != 2 && argc != 3) {
		print_error_message(SERVER_USAGE_MSG);
		return (ERROR);
	} else if (argc == 3) {
		port = (short) atoi(argv[2]);
	}

	res = initialize_users_array(&usersAmount, &users, argv[1]);
	if (res == ERROR) {
		print_error_message(INIT_USER_ARR_FAILED);
		print_error();
		return(ERROR);
	}

	res = initialize_unrecognized_users_array(&unrecognizedUsers);
	if (res == ERROR) {
		free_users_array(users, usersAmount);
		return (ERROR);
	}
	unrecognizedUsersInUse = 0;
	unrecognizedUsersSize = 1;

	if (initiallize_listen_socket(&listenSocket, port) == ERROR) {
		print_error();
		free_users_array(users, usersAmount);
		free_unrecognized_users_array(unrecognizedUsers, unrecognizedUsersSize);
		return (ERROR);
	}

	tv.tv_sec = 0;
	tv.tv_usec = SELECT_UTIMEVAL;

	do {
		/* Gathering all relevant file descriptors */
		refresh_sets(&readfds, &writefds, &errorfds, &maxfd, listenSocket, users, usersAmount,
				unrecognizedUsers, unrecognizedUsersSize);
		res = select(maxfd, &readfds, &writefds, &errorfds, &tv);
		res = handle_return_value(res);
		if (res == ERROR) {
			break;
		}

		handle_error_fds(&readfds, &writefds, &errorfds, users, unrecognizedUsers, usersAmount, unrecognizedUsersSize);

		res = handle_read_fds(&readfds, &writefds, listenSocket, users, usersAmount, &unrecognizedUsers, &unrecognizedUsersInUse,
				&unrecognizedUsersSize, &chatQueueHead);
		res = handle_return_value(res);
		if (res == ERROR) {
			break;
		}

		res = prepare_chat_messages(users, usersAmount, &chatQueueHead);
		res = handle_return_value(res);
		if (res == ERROR) {
			break;
		}

		handle_send_fds(&writefds, users, usersAmount, unrecognizedUsers, unrecognizedUsersInUse);

	} while (1);

	/* Releasing resources */
	release_chat_queue(chatQueueHead);
	init_FD_sets(&readfds, &writefds, &errorfds);
	free_unrecognized_users_array(unrecognizedUsers, unrecognizedUsersSize);
	free_users_array(users, usersAmount);
	close(listenSocket);

	return (res);
}
