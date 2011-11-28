#define MESSAGE_TYPE_MASK 0x1F
#define MESSAGE_SIZE_MASK 0xE0

#include "common.h"

void print_error() {

	fprintf(stderr, "Error: %s\n", strerror(errno));
}

void print_error_message(char* message) {

	fprintf(stderr, "Error: %s\n", message);
}

/* Source: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
int send_all(int targetSocket, unsigned char *buf, int *len) {

	int total = 0; /* How many bytes we've sent */
	int bytesleft = *len; /* How many we have left to send */
	int n;

	while (total < *len) {
		n = send(targetSocket, buf + total, bytesleft, 0);
		if (n == -1) {
			break;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; /* Return number actually sent here */

	return n == -1 ? ERROR : 0; /* return ERROR on failure, 0 on success */
}

int recv_all(int sourceSocket, unsigned char *buf, int *len) {

	int total = 0; /* How many bytes we've sent */
	int bytesleft = *len; /* How many we have left to send */
	int n;

	while (total < *len) {
		n = recv(sourceSocket, buf + total, bytesleft, 0);
		if (n == -1 || n == 0) {
			break;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; /* Return number actually sent here */

	return (n == -1 ? ERROR : (n == 0 ? ERROR_SOCKET_CLOSED : 0));
}

void free_message(Message *message) {
	if (message->data != NULL) {
		free(message->data);
	}
	memset(message, 0, sizeof(Message));
}

int send_message(int targetSocket, Message *message) {

	int bytesToSend;
	int res;
	unsigned char header;
	unsigned int len = 0;

	/* Sending header */
	bytesToSend = 1;
	header = (unsigned char)(message->messageType) | (((unsigned char)(message->messageSize))<<5);
	res = send_all(targetSocket, &header, &bytesToSend);

	/* In case of an error we stop before sending the data */
	if (res == ERROR) {
		free_message(message);
		return (ERROR);
	} else {
		len = bytesToSend;
	}

	/* Sending data size (if needed) */
	if (message->messageSize == VariedSize) {
		bytesToSend = sizeof(int);
		res = send_all(targetSocket, (unsigned char*)&(message->size), &bytesToSend);
		if (res == ERROR) {
			free_message(message);
			return (ERROR);
		}
	}

	/* Sending data (if needed) */
	if (message->messageSize != ZeroSize) {
		bytesToSend = message->size;
		res = send_all(targetSocket, message->data, &bytesToSend);
		if (res == ERROR) {
			free_message(message);
			return (ERROR);
		} else {
			len += bytesToSend;
		}
	}

	free_message(message);
	return (len == (message->size + 1) ? 0 : ERROR_LOGICAL);
}

int recv_message(int sourceSocket, Message *message) {

	int bytesToRecv;
	int res;
	unsigned char header;
	unsigned int len = 0;

	/* Receiving header */
	bytesToRecv = 1;
	res = recv_all(sourceSocket, &header, &bytesToRecv);

	/* In case of an error we stop before sending the data */
	if (res != 0) {
		return (res);
	} else {
		message->messageType = header & MESSAGE_TYPE_MASK;
		message->messageSize = (header & MESSAGE_SIZE_MASK)>>5;
		len = bytesToRecv;
	}

	/* Receiving data size (if needed) */
	if (message->messageSize == ZeroSize) {
		bytesToRecv = message->size = 0;
	} else if (message->messageSize == TwoBytes) {
		bytesToRecv = message->size = sizeof(unsigned short);
	} else if (message->messageSize == ThreeBytes) {
		bytesToRecv = message->size = 1 + sizeof(unsigned short);
	} else if (message->size == VariedSize) {
		bytesToRecv = sizeof(int);
		res = recv_all(sourceSocket, (unsigned char*)&(bytesToRecv), &bytesToRecv);
		if (res != 0) {
			return (res);
		}
		message->size = bytesToRecv;
	} else {
		return (ERROR_LOGICAL);
	}

	/* Receiving data */
	if (bytesToRecv > 0) {
		message->data = calloc(message->size, 1);
		res = recv_all(sourceSocket, message->data, &bytesToRecv);
		if (res != 0) {
			free(message->data);
			return res;
		} else {
			len += bytesToRecv;
		}
	}

	return (len == (message->size + 1) ? 0 : ERROR_LOGICAL);
}

int check_typed_message(int socket, Message *message,
		MessageType messageType) {
	int res;

	res = recv_message(socket, message);
	if (res != 0) {
		free_message(message);
		return (res);
	} else if (message->messageType != messageType) {
		free_message(message);
		return (ERROR_LOGICAL);
	}

	return (0);
}

int prepare_message_from_string(char* str, Message* message) {

	message->messageType = String;
	message->messageSize = VariedSize;
	message->size = strlen(str);

	message->data = calloc(message->size, 1);
	if (message->data != NULL) {
		memcpy(message->data, str, message->size);
	}

	return (message->data == NULL ? ERROR : 0);
}

int prepare_string_from_message(char** str, Message* message) {
	
	*str = (char*)calloc(message->size + 1, 1);

	/* The string will be valid because the calloc inputed 0 on its end */
	memcpy(*str, message->data, message->size);
	free_message(message);
	return (*str == NULL ? ERROR : 0);
}

int send_message_from_string (int socket, char *str) {

	Message message;
	int res;

	res = prepare_message_from_string(str, &message);
	if (res != 0) {
		free_message(&message);
		return (res);
	}

	res = send_message(socket, &message);
	if (res != 0) {
		return (res);
	}

	return (0);
}

int recv_string_from_message (int socket, char **str) {

	Message message;
	int res;

	res = check_typed_message(socket, &message, String);
	if (res != 0) {
		return (res);
	}

	res = prepare_string_from_message(str, &message);
	if (res != 0) {
		free_message(&message);
		return (res);
	}

	free_message(&message);
	return (0);
}

int prepare_message_from_credentials(char *userName, char *password,
		Message *message) {

	char credentials[MAX_NAME_LEN + MAX_PASSWORD_LEN + 2];
	sprintf(credentials, "%s\t%s", userName, password);

	message->messageType = Credentials;
	message->size = strlen(credentials);
	message->messageSize = VariedSize;
	message->data = (unsigned char*) calloc(message->size, 1);
	if (message->data == NULL) {
		return (ERROR);
	} else {
		memcpy(message->data, credentials, message->size);
		return (0);
	}
}

int send_message_from_credentials(int socket, char* userName, char* password) {

	Message message;
	int res;

	if (prepare_message_from_credentials(userName, password, &message) != 0) {
		free_message(&message);
		return (ERROR);
	}

	if ((res = send_message(socket, &message)) != 0) {
		return (res);
	}

	return (0);
}

int get_credentials_from_message(Message* message, char* userName, char* password) {

	char credentials[MAX_NAME_LEN + MAX_PASSWORD_LEN + 1];

	if (message->messageType != Credentials) {
		free_message(message);
		return (ERROR);
	}

	memcpy(credentials, message->data, message->size);
	credentials[message->size] = 0;
	if (sscanf((char*) message->data, "%s\t%s", userName, password) != 2) {
		free_message(message);
		return (ERROR);
	}

	free_message(message);
	return (0);
}

int send_empty_message(int socket, MessageType type) {

	Message message;

	message.messageType = type;
	message.messageSize = ZeroSize;
	message.size = 0;

	return (send_message(socket, &message));
}

void free_mail(Mail* mail) {

	int i;
	for (i = 0; (i < mail->numAttachments) && (mail->attachments != NULL); i++) {
		if (mail->attachments[i].data != NULL) {
			free(mail->attachments[i].data);
		}

		if (mail->attachments[i].fileName != NULL) {
			free(mail->attachments[i].fileName);
		}

		memset(mail->attachments + i, 0, sizeof(Attachment));
	}

	if (mail->attachments != NULL) {
		free(mail->attachments);
	}

	for (i = 0; (i < mail->numRecipients) && (mail->recipients != NULL); i++) {
		if (mail->recipients[i] != NULL) {
			free(mail->recipients[i]);
		}
		mail->recipients[i] = NULL;
	}

	if (mail->recipients != NULL) {
		free(mail->recipients);
	}

	if (mail->body != NULL) {
		free(mail->body);
	}

	if (mail->sender != NULL) {
		free(mail->sender);
	}

	if (mail->subject != NULL) {
		free(mail->subject);
	}

	memset(mail, 0, sizeof(Mail));
}

void free_mails(int mailAmount, Mail *mails) {
	int i;

	for (i = 0; i < mailAmount; i++) {
		free_mail(mails + i);
	}
}

int calculate_mail_header_size(Mail *mail) {
	int size = 0;

	size += strlen(mail->sender) + 1;
	size += strlen(mail->subject) + 1;
	size += sizeof(mail->numAttachments);

	return (size);
}

int calculate_inbox_info_size(Mail **mails, int mailAmount){

	int i, messageSize = 0;

	for (i = 0; i < mailAmount; i++) {
		if (mails[i] != NULL){
			messageSize += sizeof(mails[i]->id);
			messageSize += calculate_mail_header_size(mails[i]);
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

int prepare_message_from_inbox_content(Mail **mails, int mailAmount, Message *message) {

	int i;
	int offset = 0;

	message->messageType = InboxContent;

	/* Calculating message size */
	message->size = calculate_inbox_info_size(mails, mailAmount) + sizeof(mailAmount);
	message->messageSize = VariedSize;

	message->data = (unsigned char*)calloc(message->size, 1);
	if (message->data == NULL) {
		return (ERROR);
	}

	memcpy(message->data + offset, &mailAmount, sizeof(mailAmount));
	offset += sizeof(mailAmount);

	for (i = 0; i < mailAmount; i++) {
		if (mails[i] != NULL){

			memcpy(message->data + offset, &(mails[i]->id), sizeof(mails[i]->id));
			offset += sizeof(mails[i]->id);
			insert_mail_header_to_buffer(message->data, &offset, mails[i]);
		}
	}

	return (0);
}

int send_message_from_inbox_content(int socket, Mail **mails, int mailAmount) {

	Message message;
	int res;

	if (prepare_message_from_inbox_content(mails, mailAmount, &message) != 0) {
		free_message(&message);
		return (ERROR);
	}

	if ((res = send_message(socket, &message)) != 0) {
		return (res);
	}

	return (0);
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

int prepare_inbox_content_from_message(Message *message, Mail **mails, int *mailAmount) {

	int offset = 0;
	int i;

	/* Getting mail amount */
	memcpy(mailAmount, message->data, sizeof(*mailAmount));
	offset += sizeof(*mailAmount);

	if (*mailAmount > 0) {
		(*mails) = calloc(*mailAmount, sizeof(Mail));
		if ((*mails) == NULL) {
			return (ERROR);
		}
	}

	for (i = 0; i < *mailAmount; i++) {
		memset((*mails) + i, 0, sizeof(Mail));

		/* Prepare id */
		memcpy(&((*mails)[i].id), message->data + offset, sizeof((*mails)[i].id));
		offset += sizeof((*mails)[i].id);

		if (prepare_mail_header_from_message(message, (*mails) + i, &offset) != 0) {
			free_mails(*mailAmount, (*mails));
			free(*mails);
			return (ERROR);
		}
	}

	return (0);
}

int recv_inbox_content_from_message(int socket, Mail **mails, int *mailAmount) {

	int res;
	Message message;

	res = check_typed_message(socket, &message, InboxContent);
	if (res != 0) {
		return (res);
	}

	res = prepare_inbox_content_from_message(&message, mails, mailAmount);
	free_message(&message);
	return (res);
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

int send_mail_id_message(int clientSocket, unsigned short mailID, Message* message, MessageType messageType) {

	message->messageType = messageType;
	message->size = sizeof(mailID);
	message->messageSize = TwoBytes;

	message->data = calloc(message->size, 1);
	if (message->data == NULL) {
		return (ERROR);
	}
	memcpy(message->data, &mailID, message->size);

	return (send_message(clientSocket, message));
}

int send_get_mail_message(int clientSocket, unsigned short mailID) {

	int res;
	Message message;

	res = send_mail_id_message(clientSocket, mailID, &message, GetMail);
	free_message(&message);
	return (res);
}

void get_mail_id_from_message(Message *message, unsigned short *mailID, MessageType messageType) {

	if (message->messageType != messageType || message->messageSize != TwoBytes) {
		*mailID = ERROR_LOGICAL;
	} else {
		memcpy(mailID, message->data, message->size);
	}

	free_message(message);
}

int prepare_message_from_mail(Mail *mail, Message *message) {

	int offset = 0, i;
	int recipientNameLen, attachemntNameLen, bodyLen;

	message->messageType = MailContent;
	message->size = calculate_mail_size(mail);
	message->messageSize = VariedSize;
	message->data = calloc(message->size, 1);
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

int send_message_from_mail(int socket, Mail *mail) {

	int res;
	Message message;

	if (prepare_message_from_mail(mail, &message) != 0) {
		return (ERROR);
	}

	if ((res = send_message(socket, &message)) != 0) {
		return (res);
	}

	return (0);
}

int prepare_mail_from_message(Message *message, Mail *mail) {
	int offset = 0, i;
	int attachmentNameLen, recipientLen, bodyLength;

	memset(mail, 0, sizeof(mail));

	if (prepare_mail_header_from_message(message, mail, &offset) != 0) {
		free_mail(mail);
		return (ERROR);
	}

	/* Preparing attachments names */
	mail->attachments = calloc(mail->numAttachments, sizeof(Attachment));
	if (mail->attachments == NULL) {
		free_mail(mail);
		return (ERROR);
	}
	for (i = 0; i < mail->numAttachments; i++) {
		attachmentNameLen = strlen((char*) (message->data + offset)) + 1;
		mail->attachments[i].fileName = calloc(attachmentNameLen, 1);
		if (mail->attachments[i].fileName == NULL) {
			free_mail(mail);
			return (ERROR);
		}
		memcpy(mail->attachments[i].fileName, message->data + offset,
				attachmentNameLen);
		offset += attachmentNameLen;
	}

	/* Preparing recipients names */
	memcpy(&(mail->numRecipients), message->data + offset,
			sizeof(mail->numRecipients));
	offset += sizeof(mail->numRecipients);
	mail->recipients = calloc(mail->numRecipients, sizeof(char*));
	if (mail->recipients == NULL) {
		free_mail(mail);
		return (ERROR);
	}
	for (i = 0; i < mail->numRecipients; i++) {
		recipientLen = strlen((char*) (message->data + offset)) + 1;
		mail->recipients[i] = calloc(recipientLen, 1);
		if (mail->recipients[i] == NULL) {
			free_mail(mail);
			return (ERROR);
		}
		memcpy(mail->recipients[i], message->data + offset, recipientLen);
		offset += recipientLen;
	}

	/* Prepare body */
	bodyLength = strlen((char*) (message->data + offset)) + 1;
	mail->body = (char*) calloc(bodyLength, 1);
	if (mail->body == NULL) {
		free_mail(mail);
		return (ERROR);
	}
	memcpy(mail->body, message->data + offset, bodyLength);
	offset += bodyLength;

	return (0);
}

int recv_mail_from_message(int socket, Mail *mail) {

	int res;
	Message message;

	res = check_typed_message(socket, &message, MailContent);
	if (res != 0) {
		return (res);
	}

	res = prepare_mail_from_message(&message, mail);
	free_message(&message);
	return (res);
}

int send_get_attachment_message(int socket, unsigned short mailID,
		unsigned char attachmentID) {

	int res;
	Message message;

	message.messageType = GetAttachment;
	message.messageSize = ThreeBytes;
	message.size = sizeof(mailID) + 1;

	message.data = calloc(message.size, 1);
	if (message.data == NULL) {
		return (ERROR);
	}
	memcpy(message.data, &mailID, sizeof(mailID));
	memcpy(message.data + sizeof(mailID), &attachmentID, 1);

	res = send_message(socket, &message);
	free_message(&message);
	return (res);
}

void get_mail_attachment_id_from_message(Message *message, unsigned short *mailID, unsigned char *attachemntID) {

	if ((message->messageType != GetAttachment)
			|| (message->size != ThreeBytes)) {
		*mailID = ERROR_LOGICAL;
	} else {
		memcpy(mailID, message->data, sizeof(short));
		memcpy(attachemntID, message->data + sizeof(short), 1);
	}

	free_message(message);
}

int prepare_message_from_attachment(Attachment *attachment, Message *message) {

	message->messageType = AttachmentContent;
	message->messageSize = VariedSize;
	message->size = attachment->size + strlen(attachment->fileName) + 1;
	message->data = calloc(message->size, 1);
	if (message->data == NULL){
		return (ERROR);
	}
	memcpy(message->data, attachment->fileName, strlen(attachment->fileName) + 1);
	memcpy(message->data + strlen(attachment->fileName) + 1, attachment->data, attachment->size);

	return (0);
}

int send_message_from_attachment(int socket, Attachment *attachment) {
	int res;
	Message message;

	if (prepare_message_from_attachment(attachment, &message) != 0) {
		return (ERROR);
	}

	if ((res = send_message(socket, &message)) != 0) {
		return (res);
	}

	return (0);
}

int prepare_attachment_from_message(Message *message, Attachment *attachment) {

}

int recv_attachment_from_message(int socket, Attachment *attachment) {

	int res;
	Message message;

	res = check_typed_message(socket, &message, AttachmentContent);
	if (res != 0) {
		return (res);
	}

	res = prepare_attachment_from_message(&message, attachment);
	free_message(&message);
	return (res);
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
