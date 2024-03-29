#define MESSAGE_TYPE_MASK 0x1F
#define MESSAGE_SIZE_MASK 0xE0
#define FILE_CHUNK_SIZE (1024 * 100)

#include "protocol.h"

/* Handles the interupt socket */
int handle_interupt_socket(int interuptSocket, InteruptFunction interuptFunction) {

	fd_set readfds, errorfds;
	int res = 0;
	struct timeval tv;

	if (interuptSocket != -1 && interuptFunction != NULL) {
		init_FD_sets(&readfds, NULL, &errorfds);
		FD_SET(interuptSocket, &readfds);
		FD_SET(interuptSocket, &errorfds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		res = select(interuptSocket + 1, &readfds, NULL, &errorfds, &tv);
		if (res == -1) {
			return (ERROR);
		}

		if (FD_ISSET(interuptSocket, &errorfds)){
			return (ERROR);
		}

		if (FD_ISSET(interuptSocket, &readfds)){
			res = interuptFunction(interuptSocket);
		}
	}

	return (res);
}

/* Source: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
int send_all(int targetSocket, unsigned char *buf, int *len, int interuptSocket, InteruptFunction interuptFunction) {

	int total = 0; /* How many bytes we've sent */
	int bytesleft = *len; /* How many we have left to send */
	int n, res = 0;

	while (total < *len) {
		res = handle_interupt_socket(interuptSocket, interuptFunction);
		if (res != 0) {
			break;
		}

		/* No signal raised on error closed socket, instead it is recognized by return value */
		n = send(targetSocket, buf + total, bytesleft, MSG_NOSIGNAL);
		if (n == -1) {
			break;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; /* Return number actually sent here */

	return (res != 0 ? res : (n == -1 ? ERROR : 0)); /* return ERROR on failure, 0 on success */
}

int recv_all(int sourceSocket, unsigned char *buf, int *len, int interuptSocket, InteruptFunction interuptFunction) {

	int total = 0; /* How many bytes we've sent */
	int bytesleft = *len; /* How many we have left to send */
	int n, res = 0;

	while (total < *len) {
		res = handle_interupt_socket(interuptSocket, interuptFunction);
		if (res != 0) {
			break;
		}

		n = recv(sourceSocket, buf + total, bytesleft, 0);
		if (n == -1 || n == 0) {
			break;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; /* Return number actually sent here */

	return (res != 0 ? res : (n == -1 ? ERROR : (n == 0 ? ERROR_SOCKET_CLOSED : 0)));
}

void free_message(Message *message) {
	if (message->data != NULL) {
		free(message->data);
	}
	memset(message, 0, sizeof(Message));
}

void free_non_blocking_message(NonBlockingMessage *nbMessage) {
	free_message(&(nbMessage->message));
	memset(nbMessage, 0, sizeof(NonBlockingMessage));
}

/* Sends an attachment to stream */
int send_attachment(int targetSocket, Attachment *attachment, int interuptSocket, InteruptFunction interuptFunction) {
	int i, bytesToSend, netAttachmentSize, res;
	unsigned char buffer[FILE_CHUNK_SIZE];

	bytesToSend = sizeof(int);
	netAttachmentSize = htonl(attachment->size);
	res = send_all(targetSocket, (unsigned char*) &(netAttachmentSize),	&bytesToSend, interuptSocket, interuptFunction);
	if (res != 0) {
		return (res);
	}

	res = fseek(attachment->file, 0, SEEK_SET);
	for (i = 0; i < attachment->size; i += FILE_CHUNK_SIZE) {
		bytesToSend = fread(buffer, 1, FILE_CHUNK_SIZE, attachment->file);
		if ((bytesToSend != FILE_CHUNK_SIZE) && (bytesToSend != (attachment->size - i))) {
			return ERROR;
		}
		res = send_all(targetSocket, buffer, &bytesToSend, interuptSocket, interuptFunction);
		if (res != 0) {
			return (res);
		}
	}

	return (0);
}

int calculate_attachemnts_size(Mail *mail) {

	int i, size = 0;

	if (mail != NULL) {
		for (i = 0; i < mail->numAttachments; i++) {
			size += sizeof(mail->attachments[i].size);
			size += mail->attachments[i].size;
		}
	}

	return (size);
}

unsigned char get_message_header(Message *message) {
	return ((((unsigned char) (message->messageType))
			| (((unsigned char) (message->messageSize)) << 5)));
}

/* Send a message to the stream with attachments */
int send_message_with_attachments(int targetSocket, Message *message, Mail *mail, int interuptSocket, InteruptFunction interuptFunction) {
	int bytesToSend;
	int res, i, netMessageSize;
	unsigned char header;
	unsigned int len = 0;

	/* Sending header */
	bytesToSend = 1;
	header = get_message_header(message);
	res = send_all(targetSocket, &header, &bytesToSend, interuptSocket, interuptFunction);

	/* In case of an error we stop before sending the data */
	if (res != 0) {
		free_message(message);
		return (res);
	} else {
		len = bytesToSend;
	}

	/* Sending data size (if needed) */
	if (message->messageSize == VariedSize) {
		bytesToSend = sizeof(int);
		netMessageSize = htonl(message->size);
		res = send_all(targetSocket, (unsigned char*) &(netMessageSize), &bytesToSend, interuptSocket, interuptFunction);
		if (res != 0) {
			free_message(message);
			return (res);
		}
	}

	/* Sending data (if needed) */
	if (message->messageSize != ZeroSize) {
		bytesToSend = message->size;
		bytesToSend -= calculate_attachemnts_size(mail);

		res = send_all(targetSocket, message->data, &bytesToSend, interuptSocket, interuptFunction);
		if (res != 0) {
			free_message(message);
			return (res);
		} else {
			len += bytesToSend;
		}

		/* Checking if there are files to send */
		if (mail != NULL) {
			for (i = 0; i < mail->numAttachments; i++) {
				res = send_attachment(targetSocket, mail->attachments + i, interuptSocket, interuptFunction);
				if (res != 0) {
					free_message(message);
					return (res);
				} else {
					len += mail->attachments[i].size + sizeof(int);
				}
			}
		}
	}

	res = (len == (message->size + 1) ? 0 : ERROR_LOGICAL);
	free_message(message);
	return (res);
}

int send_message(int targetSocket, Message *message, int interuptSocket, InteruptFunction interuptFunction) {
	return send_message_with_attachments(targetSocket, message, NULL, interuptSocket, interuptFunction);
}

void set_message_header(Message *message, unsigned char header) {
	message->messageType = header & MESSAGE_TYPE_MASK;
	message->messageSize = (header & MESSAGE_SIZE_MASK)>>5;
}

int recv_attachment(int sourceSocket, Message *message, char *path, int *bytesSent,
		int interuptSocket, InteruptFunction interuptFunction) {

	int netAttachmentFileNameLength, attachmentFileNameLength, i, bytesToReceive, res;
	unsigned char buffer[FILE_CHUNK_SIZE];
	char *fullPath;
	Attachment attachment;

	memset(&attachment, 0, sizeof(Attachment));

	bytesToReceive = sizeof(netAttachmentFileNameLength);
	res = recv_all(sourceSocket, (unsigned char*)&netAttachmentFileNameLength, &bytesToReceive,
			interuptSocket, interuptFunction);
	if (res != 0) {
		return (res);
	}
	attachmentFileNameLength = ntohl(netAttachmentFileNameLength);

	attachment.fileName = calloc(attachmentFileNameLength + 1, 1);
	if (attachment.fileName == NULL) {
		return (ERROR);
	}

	bytesToReceive = attachmentFileNameLength;
	res = recv_all(sourceSocket, (unsigned char*)attachment.fileName, &bytesToReceive,
			interuptSocket, interuptFunction);
	if (res != 0) {
		free_attachment(&attachment);
		return (res);
	}

	fullPath = calloc(attachmentFileNameLength + strlen(path) + 1, 1);
	if (fullPath == NULL) {
		free_attachment(&attachment);
		return (res);
	}

	strcat(fullPath, path);
	strcat(fullPath, attachment.fileName);
	attachment.file = get_valid_file(fullPath, "w");
	if (attachment.file == NULL) {
		free(fullPath);
		free_attachment(&attachment);
		return(ERROR);
	}
	free(fullPath);

	attachment.size = message->size - sizeof(attachmentFileNameLength) - attachmentFileNameLength;

	res = fseek(attachment.file, 0, SEEK_SET);
	for (i = 0; i < attachment.size; i += FILE_CHUNK_SIZE) {
		bytesToReceive = (i + FILE_CHUNK_SIZE > attachment.size ? attachment.size - i : FILE_CHUNK_SIZE);
		res = recv_all(sourceSocket, buffer, &bytesToReceive, interuptSocket, interuptFunction);
		if (res != 0) {
			free_attachment(&attachment);
			return(ERROR);
		}

		res = fwrite(buffer, 1, bytesToReceive, attachment.file);
		if (res != bytesToReceive) {
			free_attachment(&attachment);
			return (ERROR);
		}
	}

	*bytesSent = attachment.size + strlen(attachment.fileName) + sizeof(attachmentFileNameLength);
	free_attachment(&attachment);
	return (0);
}

int recv_message_with_attachment(int sourceSocket, Message *message, char *path,
		int interuptSocket, InteruptFunction interuptFunction) {
	int bytesToRecv, netMessageSize;
	int res;
	unsigned char header;
	unsigned int len = 0;

	memset(message, 0, sizeof(Message));

	/* Receiving header */
	bytesToRecv = 1;
	res = recv_all(sourceSocket, &header, &bytesToRecv, interuptSocket, interuptFunction);

	/* In case of an error we stop before receiving the data */
	if (res != 0) {
		return (res);
	} else {
		set_message_header(message, header);
		len = bytesToRecv;
	}

	/* Receiving data size (if needed) */
	if (message->messageSize == ZeroSize) {
		bytesToRecv = message->size = 0;
	} else if (message->messageSize == TwoBytes) {
		bytesToRecv = message->size = 2;
	} else if (message->messageSize == ThreeBytes) {
		bytesToRecv = message->size = 3;
	} else if (message->messageSize == VariedSize) {
		bytesToRecv = sizeof(int);
		res = recv_all(sourceSocket, (unsigned char*)&(netMessageSize), &bytesToRecv,
					interuptSocket, interuptFunction);
		if (res != 0) {
			return (res);
		}
		message->size = ntohl(netMessageSize);
		bytesToRecv = message->size;
	} else {
		return (ERROR_LOGICAL);
	}

	/* Receiving data */
	if (bytesToRecv > 0) {
		if (message->messageType == AttachmentContent) {
			if (path == NULL) {
				return (ERROR);
			} else {
				res = recv_attachment(sourceSocket, message, path, &bytesToRecv, interuptSocket, interuptFunction);
				if (res != 0) {
					return (res);
				} else {
					len += bytesToRecv;
				}
			}
		} else {
			message->data = calloc(message->size, 1);
			if (message->data == NULL) {
				return (ERROR);
			}

			res = recv_all(sourceSocket, message->data, &bytesToRecv, interuptSocket, interuptFunction);
			if (res != 0) {
				return (res);
			} else {
				len += bytesToRecv;
			}
		}
	}

	return (len == (message->size + 1) ? 0 : ERROR_LOGICAL);
}

int recv_message(int sourceSocket, Message *message, int interuptSocket, InteruptFunction interuptFunction) {
	return (recv_message_with_attachment(sourceSocket, message, NULL, interuptSocket, interuptFunction));
}

int send_header(int targetSocket, NonBlockingMessage *nbMessage){
	int res;
	int bytesToSend = 1;
	unsigned char header = get_message_header(&(nbMessage->message));

	/* No signal raised on error closed socket, instead it is recognized by return value */
	res = send(targetSocket, &header, bytesToSend, MSG_NOSIGNAL);
	if (res == -1) {
		return (ERROR);
	} else if (res == 0) {
		return (ERROR_SOCKET_CLOSED);
	} else if (res == bytesToSend) {
		nbMessage->headerHandled = 1;
		return (0);
	} else {
		return (ERROR_LOGICAL);
	}
}

int get_buffer_from_attachments(int offset, unsigned char *buffer, Mail *mail) {
	int i, remainingSizeField, bufferLen, netAttachmentSize, curOffset = 0;

	for (i = 0; i < mail->numAttachments; i++) {
		curOffset += mail->attachments[i].size + sizeof(mail->attachments[i].size);
		/* Checking if the offset is from the current file */
		if (offset < curOffset) {
			/* Calculating offset in current file */
			curOffset -=  mail->attachments[i].size + sizeof(mail->attachments[i].size);
			curOffset = offset - curOffset;

			/* Checking if the size is needed */
			remainingSizeField = sizeof(mail->attachments[i].size) - curOffset;
			if (remainingSizeField > 0) {
				netAttachmentSize = htonl(mail->attachments[i].size);
				memcpy(buffer, ((unsigned char*)&(netAttachmentSize)) + sizeof(netAttachmentSize) - remainingSizeField, remainingSizeField);
			} else {
				remainingSizeField = 0;
			}

			fseek(mail->attachments[i].file, curOffset - remainingSizeField, SEEK_SET);
			bufferLen = fread(buffer + remainingSizeField, 1, FILE_CHUNK_SIZE - remainingSizeField, mail->attachments[i].file);
			if ((bufferLen != (FILE_CHUNK_SIZE - remainingSizeField)) && (bufferLen != (mail->attachments[i].size - (curOffset - remainingSizeField)))) {
				return ERROR;
			}

			break;
		}
	}

	return (bufferLen);
}

int send_partial_message(int targetSocket, NonBlockingMessage *nbMessage, Mail *mail) {
	int bytesToSend, res, netMessageSize, attachmentsSize;
	unsigned char buffer[FILE_CHUNK_SIZE];

	if (nbMessage->message.messageSize == VariedSize && !(nbMessage->sizeHandled)) {
		bytesToSend = - nbMessage->dataOffset;
		netMessageSize = htonl(nbMessage->message.size);
		res = send(targetSocket, ((unsigned char*)&netMessageSize) + sizeof(netMessageSize) + nbMessage->dataOffset,
				bytesToSend, MSG_NOSIGNAL);
	} else {
		attachmentsSize = calculate_attachemnts_size(mail);
		bytesToSend = nbMessage->message.size - nbMessage->dataOffset;

		if (bytesToSend > 0) {
			if (bytesToSend > attachmentsSize) {
				res = send(targetSocket, nbMessage->message.data + nbMessage->dataOffset,
						bytesToSend - attachmentsSize, MSG_NOSIGNAL);
			} else {
				res = get_buffer_from_attachments(attachmentsSize - bytesToSend, buffer, mail);
				if (res > 0) {
					res = send(targetSocket, buffer, res, MSG_NOSIGNAL);
				}
			}
		} else {
			free_non_blocking_message(nbMessage);
			return (0);
		}
	}

	if (res == -1) {
		return (ERROR);
	} else if (res == 0) {
		return (ERROR_SOCKET_CLOSED);
	} else {
		(nbMessage->dataOffset) += res;

		/* Checking if the varied size was sent */
		if ((nbMessage->message.messageSize == VariedSize) && !(nbMessage->sizeHandled) && (nbMessage->dataOffset == 0)) {
			nbMessage->sizeHandled = 1;
		} else if (nbMessage->message.size == nbMessage->dataOffset) {
			free_non_blocking_message(nbMessage);
		}
	}

	return (0);
}

/* Send a non blocking message to the stream with attachments */
int send_non_blocking_message_with_attachments(int targetSocket, NonBlockingMessage *nbMessage,
		Mail *mail) {
	int res;

	if (!(nbMessage->headerHandled)) {
		/* Receiving the beginning of the message - only header */
		res = send_header(targetSocket, nbMessage);

		nbMessage->sizeHandled = 1;
		if (nbMessage->message.messageSize == VariedSize) {
			nbMessage->dataOffset = -sizeof(nbMessage->message.size);
			nbMessage->sizeHandled = 0;
		} else if (nbMessage->message.messageSize == ZeroSize) {
			free_non_blocking_message(nbMessage);
		}
	} else if (nbMessage->isPartial) {
		/* This can change the isPartial to false, now the header is already sent */
		res = send_partial_message(targetSocket, nbMessage, mail);
	} else {
		/* Invalid state */
		res = ERROR;
	}

	return (res);
}

int send_non_blocking_message(int targetSocket, NonBlockingMessage *nbMessage) {
	return send_non_blocking_message_with_attachments(targetSocket, nbMessage, NULL);
}

int recv_header(int sourceSocket, NonBlockingMessage *nbMessage){
	unsigned char header;
	int res;
	int bytesToRecieve = 1;

	res = recv(sourceSocket, &header, bytesToRecieve, 0);
	if (res == -1) {
		return (ERROR);
	} else if (res == 0) {
		return (ERROR_SOCKET_CLOSED);
	} else if (res == bytesToRecieve) {
		set_message_header(&(nbMessage->message), header);
		nbMessage->headerHandled = 1;
		return (0);
	} else {
		return (ERROR_LOGICAL);
	}
}

int prepare_data_buffer(NonBlockingMessage *nbMessage, int size) {
	nbMessage->message.size = size;
	nbMessage->isPartial = (size == 0 ? 0 : 1);
	nbMessage->dataOffset = 0;

	if (nbMessage->message.size > 0) {
		nbMessage->message.data = calloc(nbMessage->message.size, 1);
		if (nbMessage->message.data == NULL) {
			return (ERROR);
		}
	}

	return (0);
}

/* Change the NonBlockingMessage to know what to get next according to the size */
int prepare_for_next_receive(NonBlockingMessage *nbMessage) {
	int res;

	switch (nbMessage->message.messageSize) {
	case ZeroSize:
		res = prepare_data_buffer(nbMessage, 0);
		nbMessage->sizeHandled = 1;
		break;
	case TwoBytes:
		res = prepare_data_buffer(nbMessage, 2);
		nbMessage->sizeHandled = 1;
		break;
	case ThreeBytes:
		res = prepare_data_buffer(nbMessage, 3);
		nbMessage->sizeHandled = 1;
		break;
	case VariedSize:
		res = prepare_data_buffer(nbMessage, sizeof(int));
		nbMessage->sizeHandled = 0;
		break;
	default:
		res = ERROR_LOGICAL;
	}

	return (res);
}

int recv_partial_message(int sourceSocket, NonBlockingMessage *nbMessage) {
	int bytesToRecieve, res, netMessageSize;

	bytesToRecieve = nbMessage->message.size - nbMessage->dataOffset;
	res = recv(sourceSocket, nbMessage->message.data + nbMessage->dataOffset, bytesToRecieve, 0);
	if (res == -1) {
		return (ERROR);
	} else if (res == 0) {
		return (ERROR_SOCKET_CLOSED);
	} else {
		(nbMessage->dataOffset) += res;

		if (nbMessage->message.size == nbMessage->dataOffset) {
			/* Checking if the varied size was received */
			if ((nbMessage->message.messageSize == VariedSize) && !(nbMessage->sizeHandled)) {
				/* Getting message size */
				memcpy(&netMessageSize, nbMessage->message.data, sizeof(netMessageSize));
				free(nbMessage->message.data);
				nbMessage->message.data = NULL;
				nbMessage->message.size = ntohl(netMessageSize);
				nbMessage->sizeHandled = 1;
				return (prepare_data_buffer(nbMessage, nbMessage->message.size));
			} else {
				nbMessage->isPartial = 0;
			}
		}
	}

	return 0;
}

int recv_non_blocking_message(int sourceSocket, NonBlockingMessage *nbMessage) {
	int res;

	if (!(nbMessage->headerHandled)) {
		/* Receiving the beginning of the message - only header */
		res = recv_header(sourceSocket, nbMessage);

		/* Preparing the message according to the buffer */
		if (res == 0) {
			res = prepare_for_next_receive(nbMessage);
		}
	} else if (nbMessage->isPartial) {
		/* This can change the isPartial to false, now the header is already received */
		res = recv_partial_message(sourceSocket, nbMessage);
	} else {
		/* Invalid state */
		res = ERROR;
	}

	return (res);
}

int is_full_message_received(NonBlockingMessage *nbMessage) {
	return (!nbMessage->isPartial && !nbMessage->messageInitialized);
}

int is_there_message_to_send(NonBlockingMessage *nbMessage) {
	return (nbMessage->isPartial && nbMessage->messageInitialized);
}

int handle_recv_message_res(int res, Message *message, MessageType messageType) {
	if (res != 0) {
		free_message(message);
		return (res);
	} else if (message->messageType == InvalidID) {
		free_message(message);
		return (ERROR_INVALID_ID);
	} else if (message->messageType != messageType) {
		free_message(message);
		return (ERROR_LOGICAL);
	}

	return (0);
}

int recv_typed_message(int socket, Message *message, MessageType messageType,
					   int interuptSocket, InteruptFunction interuptFunction) {
	int res;

	res = recv_message(socket, message, interuptSocket, interuptFunction);
	return (handle_recv_message_res(res, message, messageType));
}

int prepare_message_from_string(char* str, NonBlockingMessage *nbMessage) {

	nbMessage->isPartial = 1;
	nbMessage->messageInitialized = 1;
	nbMessage->message.messageType = String;
	nbMessage->message.messageSize = VariedSize;
	nbMessage->message.size = strlen(str);

	nbMessage->message.data = calloc(nbMessage->message.size, 1);
	if (nbMessage->message.data == NULL) {
		free_non_blocking_message(nbMessage);
		return (ERROR);
	}

	memcpy(nbMessage->message.data, str, nbMessage->message.size);
	return (0);
}

int prepare_string_from_message(char** str, Message* message) {

	*str = (char*)calloc(message->size + 1, 1);

	/* The string will be valid because the calloc inputed 0 on its end */
	memcpy(*str, message->data, message->size);
	free_message(message);
	return (*str == NULL ? ERROR : 0);
}

int recv_string_from_message (int socket, char **str, int interuptSocket, InteruptFunction interuptFunction) {

	Message message;
	int res;

	res = recv_typed_message(socket, &message, String, interuptSocket, interuptFunction);
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

	message->messageType = CredentialsMain;
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

void set_empty_message(NonBlockingMessage* nbMessage, MessageType type) {
	free_non_blocking_message(nbMessage);
	nbMessage->message.messageType = type;
	nbMessage->message.messageSize = ZeroSize;
	nbMessage->message.size = 0;
	nbMessage->isPartial = 1;
	nbMessage->messageInitialized = 1;
}

int send_empty_message(int socket, MessageType type, int interuptSocket, InteruptFunction interuptFunction) {

	Message message;
	int res;

	memset(&message, 0, sizeof(Message));
	message.messageType = type;
	message.messageSize = ZeroSize;
	message.size = 0;

	res = send_message(socket, &message, interuptSocket, interuptFunction);
	free_message(&message);
	return (res);
}

int send_quit_message(int socket) {
	return (send_empty_message(socket, Quit, -1, NULL));
}

int send_message_from_credentials(int socket, int chatSocket, char* userName, char* password) {

	Message message;
	int res;

	if (prepare_message_from_credentials(userName, password, &message) != 0) {
		free_message(&message);
		return (ERROR);
	}

	if ((res = send_message(socket, &message, -1, NULL)) != 0) {
		return (res);
	}

	if (prepare_message_from_credentials(userName, password, &message) != 0) {
		free_message(&message);
		return (ERROR);
	}
	message.messageType = CredentialsChat;
	if ((res = send_message(chatSocket, &message, -1, NULL)) != 0) {
		free_message(&message);
		return (res);
	}

	return (0);
}

int prepare_credentials_from_message(Message* message, char* userName, char* password) {

	char credentials[MAX_NAME_LEN + MAX_PASSWORD_LEN + 1];

	if (message->messageType != CredentialsMain && message->messageType != CredentialsChat) {
		free_message(message);
		return (ERROR);
	}

	memcpy(credentials, message->data, message->size);
	credentials[message->size] = 0;

	if (sscanf(credentials, "%s\t%s", userName, password) != 2) {
		return (ERROR);
	} else {
		return (0);
	}
}

void prepare_credentials_deny_message(NonBlockingMessage* nbMessage) {
	set_empty_message(nbMessage, CredentialsDeny);
}

void prepare_credentials_approve_message(NonBlockingMessage* nbMessage) {
	set_empty_message(nbMessage, CredentialsApprove);
}

int recv_credentials_result(int socket, int *isLoggedIn, int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	res = recv_message(socket, &message, interuptSocket, interuptFunction);
	if (res == 0) {
		if (message.messageType == CredentialsApprove) {
			*isLoggedIn = 1;
		} else if (message.messageType == CredentialsDeny) {
			*isLoggedIn = 0;
		} else {
			res = ERROR_LOGICAL;
		}
	}

	free_message(&message);
	return (res);
}

void free_attachment(Attachment *attachment) {
	if (attachment->data != NULL) {
		free(attachment->data);
	}

	if (attachment->fileName != NULL) {
		free(attachment->fileName);
	}

	if (attachment->file != NULL) {
		fclose(attachment->file);
	}

	memset(attachment, 0, sizeof(Attachment));
}

void free_mail(Mail* mail) {

	int i;
	for (i = 0; (i < mail->numAttachments) && (mail->attachments != NULL); i++) {
		free_attachment(mail->attachments + i);
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
		if (mails + i != NULL) {
			free_mail(mails + i);
		}
	}
}

int calculate_mail_header_size(Mail *mail) {
	int size = 0;

	if (mail->sender != NULL) {
		size += strlen(mail->sender) + 1;
	}
	if (mail->subject != NULL) {
		size += strlen(mail->subject) + 1;
	}
	size += sizeof(mail->numAttachments);

	return (size);
}

int calculate_inbox_info_size(Mail **mails, int mailAmount){

	int i, messageSize = 0;

	for (i = 0; i < mailAmount; i++) {
		if (mails[i] != NULL){
			messageSize += sizeof(mails[i]->clientId);
			messageSize += calculate_mail_header_size(mails[i]);
		}
	}
	return (messageSize);
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

unsigned short calculate_non_empty_mail_amount(Mail **mails, int mailAmount) {

	unsigned short i, amount = 0;

	for (i = 0; i < mailAmount; i++) {
		if (mails[i] != NULL) {
			amount++;
		}
	}

	return (amount);
}

int prepare_message_from_inbox_content(Mail **mails, unsigned short mailAmount, NonBlockingMessage* nbMessage) {

	int i;
	int offset = 0;
	unsigned short netNonEmptyMails, netClientID, nonEmptyMails;

	nonEmptyMails = calculate_non_empty_mail_amount(mails, mailAmount);
	nbMessage->isPartial = 1;
	nbMessage->messageInitialized = 1;
	nbMessage->message.messageType = InboxContent;

	/* Calculating message size */
	nbMessage->message.size = calculate_inbox_info_size(mails, mailAmount) + sizeof(nonEmptyMails);
	nbMessage->message.messageSize = VariedSize;

	nbMessage->message.data = (unsigned char*)calloc(nbMessage->message.size, 1);
	if (nbMessage->message.data == NULL) {
		free_non_blocking_message(nbMessage);
		return (ERROR);
	}

	netNonEmptyMails = htons(nonEmptyMails);
	memcpy(nbMessage->message.data + offset, &netNonEmptyMails, sizeof(netNonEmptyMails));
	offset += sizeof(mailAmount);

	for (i = 0; i < mailAmount; i++) {
		if (mails[i] != NULL) {

			netClientID = htons(mails[i]->clientId);
			memcpy(nbMessage->message.data + offset, &netClientID, sizeof(netClientID));
			offset += sizeof(mails[i]->clientId);
			insert_mail_header_to_buffer(nbMessage->message.data, &offset, mails[i]);
		}
	}

	return (0);
}

int send_show_inbox_message(int socket, int interuptSocket, InteruptFunction interuptFunction) {
	return (send_empty_message(socket, ShowInbox, interuptSocket, interuptFunction));
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

int prepare_inbox_content_from_message(Message *message, Mail **mails, unsigned short *mailAmount) {

	int offset = 0;
	int i;
	unsigned short netMailAmount, netClientID;

	/* Getting mail amount */
	memcpy(&netMailAmount, message->data + offset, sizeof(netMailAmount));
	*mailAmount = ntohs(netMailAmount);
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
		memcpy(&netClientID, message->data + offset, sizeof(netClientID));
		offset += sizeof((*mails)[i].clientId);
		(*mails)[i].clientId = ntohs(netClientID);

		if (prepare_mail_header_from_message(message, (*mails) + i, &offset) != 0) {
			free_mails(*mailAmount, (*mails));
			free(*mails);
			return (ERROR);
		}
	}

	return (0);
}

int recv_inbox_content_from_message(int socket, Mail **mails, unsigned short *mailAmount,
									int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	res = recv_typed_message(socket, &message, InboxContent, interuptSocket, interuptFunction);
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

int send_mail_id_message(int clientSocket, unsigned short mailID, Message* message, MessageType messageType,
						 int interuptSocket, InteruptFunction interuptFunction) {

	unsigned short netMailID;

	message->messageType = messageType;
	message->size = sizeof(netMailID);
	message->messageSize = TwoBytes;

	message->data = calloc(message->size, 1);
	if (message->data == NULL) {
		return (ERROR);
	}
	netMailID = htons(mailID);
	memcpy(message->data, &netMailID, message->size);

	return (send_message(clientSocket, message, interuptSocket, interuptFunction));
}

int send_get_mail_message(int socket, unsigned short mailID, int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	res = send_mail_id_message(socket, mailID, &message, GetMail, interuptSocket, interuptFunction);
	free_message(&message);
	return (res);
}

unsigned short prepare_mail_id_from_message(NonBlockingMessage *nbMessage, MessageType messageType) {

	unsigned short netMailID, mailID;

	if (nbMessage->message.messageType != messageType || nbMessage->message.messageSize != TwoBytes) {
		mailID = ERROR_LOGICAL;
	} else {
		memcpy(&netMailID, nbMessage->message.data, nbMessage->message.size);
		mailID = ntohs(netMailID);
	}

	free_non_blocking_message(nbMessage);
	return (mailID);
}

void insert_mail_contents_to_buffer(unsigned char *buffer, int *offset, Mail *mail) {

	int i;
	int recipientNameLen, attachemntNameLen, bodyLen;

	/* Inserting attachments names */
	for (i = 0; i < mail->numAttachments; i++) {
		attachemntNameLen = strlen(mail->attachments[i].fileName) + 1;
		memcpy(buffer + *offset, mail->attachments[i].fileName,
				attachemntNameLen);
		*offset += attachemntNameLen;
	}

	/* Inserting recipients */
	memcpy(buffer + *offset, &(mail->numRecipients),
			sizeof(mail->numRecipients));
	*offset += sizeof(mail->numRecipients);
	for (i = 0; i < mail->numRecipients; i++) {
		recipientNameLen = strlen(mail->recipients[i]) + 1;
		memcpy(buffer + *offset, mail->recipients[i], recipientNameLen);
		*offset += recipientNameLen;
	}

	/* Inserting body */
	bodyLen = strlen(mail->body) + 1;
	memcpy(buffer + *offset, mail->body, bodyLen);
	*offset += bodyLen;
}

int prepare_message_from_mail(Mail *mail, NonBlockingMessage *nbMessage, int isChat) {

	int offset = 0;

	nbMessage->isPartial = 1;
	nbMessage->messageInitialized = 1;
	nbMessage->message.messageType = isChat ? ChatMessageReceive : MailContent;
	nbMessage->message.size = calculate_mail_size(mail);
	nbMessage->message.messageSize = VariedSize;
	nbMessage->message.data = calloc(nbMessage->message.size, 1);
	if (nbMessage->message.data == NULL) {
		free_non_blocking_message(nbMessage);
		return (ERROR);
	}

	insert_mail_header_to_buffer(nbMessage->message.data, &offset, mail);

	insert_mail_contents_to_buffer(nbMessage->message.data, &offset, mail);

	return (0);
}

void prepare_invalid_id_message(NonBlockingMessage *nbMessage) {
	set_empty_message(nbMessage, InvalidID);
}

int prepare_mail_from_message(Message *message, Mail *mail, int *offset) {
	int i;
	int attachmentNameLen, recipientLen, bodyLength;

	memset(mail, 0, sizeof(Mail));

	if (prepare_mail_header_from_message(message, mail, offset) != 0) {
		free_mail(mail);
		return (ERROR);
	}

	/* Preparing attachments names */
	if (mail->numAttachments > 0) {
		mail->attachments = calloc(mail->numAttachments, sizeof(Attachment));
		if (mail->attachments == NULL) {
			free_mail(mail);
			return (ERROR);
		}
		for (i = 0; i < mail->numAttachments; i++) {
			attachmentNameLen = strlen((char*) (message->data + *offset)) + 1;
			mail->attachments[i].fileName = calloc(attachmentNameLen, 1);
			if (mail->attachments[i].fileName == NULL) {
				free_mail(mail);
				return (ERROR);
			}
			memcpy(mail->attachments[i].fileName, message->data + *offset,
					attachmentNameLen);
			*offset += attachmentNameLen;
		}
	}

	/* Preparing recipients names */
	memcpy(&(mail->numRecipients), message->data + *offset,
			sizeof(mail->numRecipients));
	*offset += sizeof(mail->numRecipients);
	if (mail->numRecipients > 0) {
		mail->recipients = calloc(mail->numRecipients, sizeof(char*));
		if (mail->recipients == NULL) {
			free_mail(mail);
			return (ERROR);
		}
		for (i = 0; i < mail->numRecipients; i++) {
			recipientLen = strlen((char*) (message->data + *offset)) + 1;
			mail->recipients[i] = calloc(recipientLen, 1);
			if (mail->recipients[i] == NULL) {
				free_mail(mail);
				return (ERROR);
			}
			memcpy(mail->recipients[i], message->data + *offset, recipientLen);
			*offset += recipientLen;
		}
	}

	/* Prepare body */
	bodyLength = strlen((char*) (message->data + *offset)) + 1;
	mail->body = (char*) calloc(bodyLength, 1);
	if (mail->body == NULL) {
		free_mail(mail);
		return (ERROR);
	}
	memcpy(mail->body, message->data + *offset, bodyLength);
	*offset += bodyLength;

	return (0);
}

int recv_mail_from_message(int socket, Mail *mail, int interuptSocket, InteruptFunction interuptFunction) {

	int res, offset = 0;
	Message message;

	res = recv_typed_message(socket, &message, MailContent, interuptSocket, interuptFunction);
	if (res != 0) {
		return (res);
	}

	res = prepare_mail_from_message(&message, mail, &offset);
	free_message(&message);
	return (res);
}

int send_get_attachment_message(int socket, unsigned short mailID, unsigned char attachmentID,
								int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;
	unsigned short netMailID;

	message.messageType = GetAttachment;
	message.messageSize = ThreeBytes;
	message.size = 3;

	message.data = calloc(message.size, 1);
	if (message.data == NULL) {
		return (ERROR);
	}
	netMailID = htons(mailID);
	memcpy(message.data, &netMailID, sizeof(netMailID));
	memcpy(message.data + sizeof(netMailID), &attachmentID, 1);

	res = send_message(socket, &message, interuptSocket, interuptFunction);
	free_message(&message);
	return (res);
}

void prepare_mail_attachment_id_from_message(NonBlockingMessage *nbMessage, unsigned short *mailID, unsigned char *attachmentID) {

	unsigned short netMailID;

	if ((nbMessage->message.messageType != GetAttachment)
			|| (nbMessage->message.messageSize != ThreeBytes)) {
		*mailID = ERROR_LOGICAL;
	} else {
		memcpy(&netMailID, nbMessage->message.data, sizeof(netMailID));
		*mailID = ntohs(netMailID);
		memcpy(attachmentID, nbMessage->message.data + sizeof(netMailID), 1);
	}

	free_non_blocking_message(nbMessage);
}

int prepare_message_from_attachment(Attachment *attachment, NonBlockingMessage *nbMessage) {

	int netAttachmentFileNameLength;

	nbMessage->isPartial = 1;
	nbMessage->messageInitialized = 1;
	nbMessage->message.messageType = AttachmentContent;
	nbMessage->message.messageSize = VariedSize;
	nbMessage->message.size = attachment->size + sizeof(netAttachmentFileNameLength) + strlen(attachment->fileName);
	nbMessage->message.data = calloc(nbMessage->message.size, 1);
	if (nbMessage->message.data == NULL) {
		free_non_blocking_message(nbMessage);
		return (ERROR);
	}

	netAttachmentFileNameLength = htonl(strlen(attachment->fileName));
	memcpy(nbMessage->message.data, &netAttachmentFileNameLength, sizeof(netAttachmentFileNameLength));
	memcpy(nbMessage->message.data + sizeof(netAttachmentFileNameLength), attachment->fileName, strlen(attachment->fileName));
	memcpy(nbMessage->message.data + sizeof(netAttachmentFileNameLength) + strlen(attachment->fileName), attachment->data, attachment->size);

	return (0);
}

int recv_attachment_file_from_message(int socket, char* attachmentPath,
									  int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	res = recv_message_with_attachment(socket, &message, attachmentPath, interuptSocket, interuptFunction);
	res = handle_recv_message_res(res, &message, AttachmentContent);
	free_message(&message);
	return (res);
}

int send_delete_mail_message(int socket, unsigned short mailID, int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	res = send_mail_id_message(socket, mailID, &message, DeleteMail, interuptSocket, interuptFunction);
	free_message(&message);
	return (res);
}

void prepare_delete_approve_message(NonBlockingMessage *nbMessage) {
	set_empty_message(nbMessage, DeleteApprove);
}

int recv_delete_result(int socket, int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	res = recv_typed_message(socket, &message, DeleteApprove, interuptSocket, interuptFunction);
	if (res != 0) {
		return (res);
	} else {
		free_message(&message);
		return (0);
	}
}

int prepare_compose_message_from_mail(Mail *mail, Message *message, int isChatMessage) {

	int offset = 0;

	message->messageType = isChatMessage ? ChatMessageSend : Compose;
	message->size = calculate_mail_size(mail);
	message->messageSize = VariedSize;
	message->data = calloc(message->size, 1);
	if (message->data == NULL) {
		return (ERROR);
	}

	/* Attachments not part of the buffer */
	message->size += calculate_attachemnts_size(mail);

	insert_mail_header_to_buffer(message->data, &offset, mail);

	insert_mail_contents_to_buffer(message->data, &offset, mail);

	return (0);
}

int send_compose_message_from_mail(int socket, Mail *mail, int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	if (prepare_compose_message_from_mail(mail, &message, 0) != 0) {
		return (ERROR);
	}

	if ((res = send_message_with_attachments(socket, &message, mail, interuptSocket, interuptFunction)) != 0) {
		return (res);
	}

	return (0);
}

int send_chat_from_mail(int socket, Mail *mail, int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	if (prepare_compose_message_from_mail(mail, &message, 1) != 0) {
		return (ERROR);
	}

	if ((res = send_message(socket, &message, interuptSocket, interuptFunction)) != 0) {
		return (res);
	}

	return (0);
}

int recv_chat_from_message(int socket, Mail *ChatMessage) {

	int res, offset = 0;
	Message message;

	res = recv_typed_message(socket, &message, ChatMessageReceive, -1, NULL);
	if (res != 0) {
		return (res);
	}

	res = prepare_mail_from_message(&message, ChatMessage, &offset);
	free_message(&message);
	return (res);
}

void prepare_chat_mail_confirm_message(NonBlockingMessage *nbMessage) {
	set_empty_message(nbMessage, ChatMailConfirm);
}

void prepare_chat_confirm_message(NonBlockingMessage *nbMessage) {
	set_empty_message(nbMessage, ChatConfirm);
}

int prepare_mail_attachments_from_message(Message *message, Mail *mail, int *offset) {

	int i, netAttachmentSize;

	/* Getting attachments data */
	for (i = 0; i < mail->numAttachments; i++) {
		memcpy(&(netAttachmentSize), message->data + *offset,
				sizeof(netAttachmentSize));
		*offset += sizeof(netAttachmentSize);
		mail->attachments[i].size = ntohl(netAttachmentSize);

		mail->attachments[i].data = calloc(mail->attachments[i].size, 1);
		if (mail->attachments[i].data == NULL) {
			free_mail(mail);
			return (ERROR);
		}
		memcpy(mail->attachments[i].data, message->data + *offset,
				mail->attachments[i].size);
		*offset += mail->attachments[i].size;
	}

	return (0);
}

int prepare_mail_from_compose_message(NonBlockingMessage *nbMessage, Mail **mail) {

	int res, offset = 0;

	*mail = calloc(1, sizeof(Mail));
	if (*mail == NULL) {
		free_non_blocking_message(nbMessage);
		return (ERROR);
	}

	res = prepare_mail_from_message(&(nbMessage->message), *mail, &offset);
	if (res != 0) {
		free_non_blocking_message(nbMessage);
		return (res);
	}

	res = prepare_mail_attachments_from_message(&(nbMessage->message), *mail, &offset);
	if (res != 0) {
		free_non_blocking_message(nbMessage);
		return (res);
	}

	free_non_blocking_message(nbMessage);
	return (0);
}

int recv_chat_result(int socket, int *isMailSent, int interuptSocket, InteruptFunction interuptFunction) {
	int res;
	Message message;

	res = recv_message(socket, &message, interuptSocket, interuptFunction);
	if (res != 0) {
		return (res);
	} else {
		if (message.messageType == ChatConfirm) {
			*isMailSent = 0;
		} else if (message.messageType == ChatMailConfirm) {
			*isMailSent = 1;
		} else {
			res = ERROR_LOGICAL;
		}

		free_message(&message);
		return (res);
	}
}

void prepare_send_approve_message(NonBlockingMessage *nbMessage) {
	set_empty_message(nbMessage, SendApprove);
}

int recv_send_result(int socket, int interuptSocket, InteruptFunction interuptFunction) {

	int res;
	Message message;

	res = recv_typed_message(socket, &message, SendApprove, interuptSocket, interuptFunction);
	if (res != 0) {
		return (res);
	} else {
		free_message(&message);
		return (0);
	}
}

void prepare_invalid_command_message(NonBlockingMessage *nbMessage) {
	set_empty_message(nbMessage, InvalidCommand);
}

int prepare_online_users_message(NonBlockingMessage *nbMessage, char **onlineUsersNames, int usersAmount) {

	int i, concatSize, netUsersAmount;

	concatSize = 0;
	for (i = 0; i < usersAmount; i++) {
		concatSize += strlen(onlineUsersNames[i]) + 1;
	}
	concatSize += sizeof(netUsersAmount);

	nbMessage->message.messageType = OnlineUsers;
	nbMessage->message.messageSize = VariedSize;
	nbMessage->message.size = concatSize;
	nbMessage->isPartial = 1;
	nbMessage->messageInitialized = 1;

	nbMessage->message.data = calloc(concatSize, 1);
	if (nbMessage->message.data == NULL) {
		free_non_blocking_message(nbMessage);
		return (ERROR);
	}

	netUsersAmount = htonl(usersAmount);
	memcpy(nbMessage->message.data, &netUsersAmount, sizeof(netUsersAmount));

	for (i = 0; i < usersAmount; i++) {
		strcat((char*)(nbMessage->message.data + sizeof(netUsersAmount)), onlineUsersNames[i]);

		if (i < (usersAmount - 1)) {
			strcat((char*)(nbMessage->message.data + sizeof(netUsersAmount)), "\t");
		}
	}

	return (0);
}

int send_show_online_users(int socket, int interuptSocket, InteruptFunction interuptFunction) {
	return (send_empty_message(socket, ShowOnlineUsers, interuptSocket, interuptFunction));
}

int prepare_online_users_from_message(Message *message, char ***onlineUsersNames, int *usersAmount){
	int i;
	char *allUsers;
	int netUsersAmount;

	memcpy(&netUsersAmount, message->data, sizeof(int));
	*usersAmount = ntohl(netUsersAmount);

	if (*usersAmount > 0) {
		*onlineUsersNames = calloc(*usersAmount, sizeof(char*));
		if (*onlineUsersNames == NULL) {
			return (ERROR);
		}

		allUsers = calloc(message->size - sizeof(netUsersAmount), sizeof(char));
		if (allUsers == NULL) {
			free(*onlineUsersNames);
			return (ERROR);
		}

		memcpy(allUsers, message->data + sizeof(netUsersAmount), message->size - sizeof(netUsersAmount));

		(*onlineUsersNames)[0] = strtok(allUsers, "\t");
		for (i = 1; i < *usersAmount; i++) {
			(*onlineUsersNames)[i] = strtok(NULL, "\t");
		}
	}

	return (0);
}

int recv_online_users(int socket, char*** onlineUsersNames, int *usersAmount, int interuptSocket, InteruptFunction interuptFunction) {
	int res;
	Message message;

	res = recv_typed_message(socket, &message, OnlineUsers, interuptSocket, interuptFunction);
	if (res != 0) {
		return (res);
	}

	res = prepare_online_users_from_message(&message, onlineUsersNames, usersAmount);
	free_message(&message);
	return (res);
}

void free_online_users_names(char **onlineUsersNames) {
	if (onlineUsersNames[0] != NULL){
		free(onlineUsersNames[0]);
	}
}
