#define MAX_ROW_LENGTH 256
#define WELLCOME_MESSAGE "Welcome! I am simple-mail-server."
#define SERVER_USAGE_MSG "Usage mail_server <users_file> [port]"
#define DEAFULT_PORT 6423

#include "common.h"

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

	user->mails = calloc(3, sizeof(Mail*));
	user->mails[0] = calloc(sizeof(Mail), 1);
	*(user->mails[0]) = mail1;
	user->mails[1] = NULL;
	user->mails[2] = calloc(sizeof(Mail), 1);
	*(user->mails[2]) = mail2;
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
		return (ERROR);
	}

	*usersAmount = count_rows(usersFile);
	*users = (User*) calloc(*usersAmount, sizeof(User));

	fseek(usersFile, 0, SEEK_SET);

	for (i = 0; i < *usersAmount; i++) {

		if (fscanf(usersFile, "%s\t%s", (*users)[i].name, (*users)[i].password) != 2) {
			fclose(usersFile);
			return (ERROR);
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
			free_mail_struct(users[i].mails[j]);
		}
		free(users[i].mails);
	}

	free(users);
}

User* check_credentials_message(User* users, int usersAmount, Message *message) {

	char userName[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	char credentials[MAX_NAME_LEN + MAX_PASSWORD_LEN + 1];

	int i;

	if (message->messageType != Credentials) {
		return (NULL);
	}

	memcpy(credentials, message->data, message->dataSize);
	credentials[message->dataSize] = 0;
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

int calculate_mail_header_size(Mail *mail) {
	int size = 0;

	size += strlen(mail->sender) + 1;
	size += strlen(mail->subject) + 1;
	size += sizeof(mail->numAttachments);

	return (size);
}

int calculate_mail_size(Mail *mail) {
	int i, size = 0;

	size += calculate_mail_header_size(mail);

	for (i = 0; i < mail->numAttachments; i++) {
		size += strlen(mail->attachments[i].fileName) + 1;
	}

	size += sizeof(mail->numRecipients);
	for (i = 0; i < mail->numRecipients; i++) {
		size += strlen(mail->recipients[i]) + 1;
	}

	size += strlen(mail->body) + 1;

	return (size);
}

int calculate_inbox_info_size(User *user){

	int i, messageSize = 0;

	for (i = 0; i < user->mailAmount; i++) {
		if (user->mails[i] != NULL){
			messageSize += sizeof(user->mails[i]->id);
			messageSize += calculate_mail_header_size(user->mails[i]);
		}
	}
	return messageSize;
}

void insert_mail_header_to_buffer(unsigned char *buffer, int *offset, Mail *mail) {
	int senderLength = strlen(mail->sender) + 1;
	int subjectLength = strlen(mail->subject) + 1;

	memcpy(buffer + *offset, mail->sender, senderLength);
	*offset += senderLength;
	memcpy(buffer + *offset, mail->subject, subjectLength);
	*offset += subjectLength;
	memcpy(buffer + *offset, &(mail->numAttachments), sizeof(mail->numAttachments));
	*offset += sizeof(mail->numAttachments);
}

int prepare_message_from_inbox_content(User *user, Message *message) {

	int i;
	int offset = 0;

	message->messageType = InboxContent;

	/* Calculating message size */
	message->dataSize = calculate_inbox_info_size(user);

	message->data = (unsigned char*)calloc(message->dataSize, 1);
	if (message->data == NULL) {
		return (ERROR);
	}

	for (i = 0; i < user->mailAmount; i++) {
		if (user->mails[i] != NULL){

			memcpy(message->data + offset, &(user->mails[i]->id), sizeof(user->mails[i]->id));
			offset += sizeof(user->mails[i]->id);
			insert_mail_header_to_buffer(message->data, &offset, user->mails[i]);
		}
	}

	return (0);
}

void get_mail_id_from_message(Message *message, unsigned short *mailID, MessageType messageType) {

	if (message->messageType != messageType || message->dataSize != sizeof(short)) {
		*mailID = ERROR_LOGICAL;
	} else {
		memcpy(mailID, message->data, message->dataSize);
		free_message(message);
	}
}

Mail* get_mail_by_id (User *user, unsigned short mailID, int *mailIndex) {

	int i;

	for (i = 0; i < user->mailAmount; i++) {
		if ((user->mails[i] != NULL) && (user->mails[i]->id == mailID)) {
			*mailIndex = i;
			return (user->mails[i]);
		}
	}

	return (NULL);
}

int prepare_message_from_mail(User *user, Message *message, unsigned short mailID) {

	int mailIndex;
	Mail* mail = get_mail_by_id(user, mailID, &mailIndex);
	int offset = 0, i;
	int recipientNameLen, attachemntNameLen, bodyLen;

	if (mail == NULL) {
		return (ERROR_INVALID_ID);
	}

	message->messageType = MailContent;
	message->dataSize = calculate_mail_size(mail);
	message->data = calloc(message->dataSize, 1);
	if (message->data == NULL) {
		return (ERROR);
	}

	insert_mail_header_to_buffer(message->data, &offset, mail);

	/* Inserting attachments names */
	for (i = 0; i < mail->numAttachments; i++) {
		attachemntNameLen = strlen(mail->attachments[i].fileName) + 1;
		memcpy(message->data + offset, mail->attachments[i].fileName, attachemntNameLen);
		offset += attachemntNameLen;
	}

	/* Inserting recipients */
	memcpy(message->data + offset, &(mail->numRecipients), sizeof(mail->numRecipients));
	offset += sizeof(mail->numRecipients);
	for (i = 0; i < mail->numRecipients; i++) {
		recipientNameLen = strlen(mail->recipients[i]) + 1;
		memcpy(message->data + offset, mail->recipients[i], recipientNameLen);
		offset += recipientNameLen;
	}

	/* Inserting body */
	bodyLen = strlen(mail->body) + 1;
	memcpy(message->data + offset, mail->body, bodyLen);
	offset += bodyLen;

	return (0);
}

void get_mail_attachment_id_from_message(Message *message, unsigned short *mailID, unsigned char *attachemntID) {

	if ((message->messageType != GetAttachment) ||
			(message->dataSize != (sizeof(short) + 1))) {
		*mailID = ERROR_LOGICAL;
	} else {
		memcpy(mailID, message->data, sizeof(short));
		memcpy(attachemntID, message->data + sizeof(short), 1);
		free_message(message);
	}
}

Attachment* get_attachment_by_id (User *user, short mailID, unsigned char attachmentID) {
	int mailIndex;
	Mail* mail = get_mail_by_id(user, mailID, &mailIndex);

	if ((mail != NULL) && (mail->numAttachments >= attachmentID)) {
		return (mail->attachments + attachmentID - 1);
	}

	return (NULL);
}

int prepare_message_from_attachment(User *user, Message *message, unsigned short mailID, unsigned char attachmentID) {
	Attachment *attachment;

	attachment = get_attachment_by_id(user, mailID, attachmentID);
	if (attachment == NULL) {
		return (ERROR_INVALID_ID);
	}

	message->messageType = AttachmentContent;
	message->dataSize = attachment->size + strlen(attachment->fileName) + 1;
	message->data = calloc(message->dataSize, 1);
	if (message->data == NULL){
		return (ERROR);
	}
	memcpy(message->data, attachment->fileName, strlen(attachment->fileName) + 1);
	memcpy(message->data + strlen(attachment->fileName) + 1, attachment->data, attachment->size);

	return (0);
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
		free_mail_struct(mail);
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

/* TODO: make sure whenever error this returns -1 */
int main(int argc, char** argv) {

	/* Variables declaration */
	short port = DEAFULT_PORT;
	int usersAmount, res;
	unsigned int len;
	User *users = NULL, *curUser = NULL;
	int listenSocket, clientSocket;
	struct sockaddr_in clientAddr;
	Message message;
	unsigned short mailID;
	unsigned char attachemntID;

	/* Validate number of arguments */
	if (argc != 2 && argc != 3) {
		print_error_message(SERVER_USAGE_MSG);
		return (ERROR);
	} else if (argc == 3) {
		port = (short) atoi(argv[2]);
	}

	res = initialliaze_users_array(&usersAmount, &users, argv[1]);
	if (res == ERROR) {
		print_error_message("Failed initiallizing users array");
	}

	if (initiallize_listen_socket(&listenSocket, port) == ERROR) {
		print_error();
		free_users_array(users, usersAmount);
		return (ERROR);
	}

	memset(&message, 0, sizeof(message));;

	do {

		/* Prepare structure for client address */
		len = sizeof(clientAddr);

		/* Start waiting till client connect */
		clientSocket = accept(listenSocket, (struct sockaddr*) &clientAddr, &len);
		if (clientSocket == -1) {
			print_error();
		} else {

			if ((prepare_message_from_string(WELLCOME_MESSAGE, &message) != 0)
					|| (send_message(clientSocket, &message) != 0)) {
				print_error();
			} else {

				free_message(&message);

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
							break;
						}
					} else if (message.messageType == ShowInbox) {
						if ((prepare_message_from_inbox_content(curUser, &message) != 0) ||
								(send_message(clientSocket, &message) != 0)) {
							print_error();
						}
					} else if (message.messageType == GetMail) {

						get_mail_id_from_message(&message, &mailID, GetMail);
						if (mailID == ERROR_LOGICAL) {
							print_error_message(INVALID_DATA_MESSAGE);
							break;
						}

						res = prepare_message_from_mail(curUser, &message, mailID);
						if (res == ERROR_INVALID_ID) {
							send_empty_message(clientSocket, InvalidID);
						} else if ((res != 0) || (send_message(clientSocket, &message) != 0)) {
							print_error();
						}
					} else if (message.messageType == GetAttachment) {

						get_mail_attachment_id_from_message(&message, &mailID, &attachemntID);
						if (mailID == ERROR_LOGICAL) {
							print_error_message(INVALID_DATA_MESSAGE);
							break;
						}

						res = prepare_message_from_attachment(curUser, &message,
								mailID, attachemntID);
						if (res == ERROR_INVALID_ID) {
							send_empty_message(clientSocket, InvalidID);
						} else if ((res != 0) || (send_message(clientSocket,
								&message) != 0)) {
							print_error();
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
					} else {
						/* TODO: send error command message */
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
