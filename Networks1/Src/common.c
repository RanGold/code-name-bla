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

int send_message(int targetSocket, Message *message) {
	int bytesToSend, headerSize;
	int res;
	unsigned char* header;
	unsigned int len = 0;

	/* Sending header */
	headerSize = bytesToSend = sizeof(int) + 1;
	header = calloc(bytesToSend, 1);
	if (header == NULL) {
		return (ERROR);
	}
	memcpy(header, (&(message->messageType)), 1);
	memcpy(header + 1, (&(message->dataSize)), sizeof(int));
	res = send_all(targetSocket, header, &bytesToSend);
	free(header);

	/* In case of an error we stop before sending the data */
	if (res == ERROR) {
		return (ERROR);
	} else {
		len = bytesToSend;
	}

	/* Sending data */
	bytesToSend = message->dataSize;
	if (bytesToSend > 0) {
		res = send_all(targetSocket, message->data, &bytesToSend);
		if (res == ERROR) {
			return (ERROR);
		} else {
			len += bytesToSend;
		}
	}

	return (len == (message->dataSize + headerSize) ? 0 : ERROR_LOGICAL);
}

int recv_message(int sourceSocket, Message *message) {
	int bytesToRecv, headerSize;
	int res;
	unsigned char* header;
	unsigned int len = 0;

	/* Receiving header */
	headerSize = bytesToRecv = sizeof(int) + 1;
	header = calloc(bytesToRecv, 1);
	if (header == NULL) {
		return (ERROR);
	}
	res = recv_all(sourceSocket, header, &bytesToRecv);

	/* In case of an error we stop before sending the data */
	if (res != 0) {
		free(header);
		return (res);
	} else {
		message->messageType = 0;
		memcpy((&(message->messageType)), header, 1);
		memcpy((&(message->dataSize)), header + 1, sizeof(message->dataSize));
		free(header);
		len = bytesToRecv;
	}

	/* Receiving data */
	bytesToRecv = message->dataSize;
	if (bytesToRecv > 0) {
		message->data = calloc(message->dataSize, 1);
		res = recv_all(sourceSocket, message->data, &bytesToRecv);
		if (res != 0) {
			free(message->data);
			return res;
		} else {
			len += bytesToRecv;
		}
	}

	return (len == (message->dataSize + headerSize) ? 0 : ERROR_LOGICAL);
}

int prepare_message_from_string(char* str, Message* message) {
	message->messageType = String;
	message->dataSize = strlen(str);

	message->data = calloc(message->dataSize, 1);
	if (message->data != NULL) {
		memcpy(message->data, str, message->dataSize);
	}

	return (message->data == NULL ? ERROR : 0);
}

int prepare_string_from_message(char** str, Message* message) {
	
	*str = (char*)calloc(message->dataSize + 1, 1);
	/* The string will be valid because the calloc inputed 0 on its end */
	memcpy(*str, message->data, message->dataSize);
	free_message(message);
	return (*str == NULL ? ERROR : 0);
}

int send_empty_message(int socket, MessageType type) {

	Message message;

	message.messageType = type;
	message.dataSize = 0;

	return (send_message(socket, &message));
}

void free_mail_struct(Mail* mail) {

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

void free_message(Message *message) {
	if (message->data != NULL) {
		free(message->data);
	}
	memset(message, 0, sizeof(Message));
}