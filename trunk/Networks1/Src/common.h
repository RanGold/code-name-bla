#define MAX_NAME_LEN 50
#define MAX_PASSWORD_LEN 50
#define MAX_EMAILS 32000

/* Errors */
#define ERROR -1
#define ERROR_SOCKET_CLOSED -2
#define ERROR_LOGICAL -3
#define INVALID_DATA_MESSAGE "Invalid data received"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

typedef enum MessageType {
	String,
	Quit,
	Credentials,
	CredentialsAccept,
	CredentialsDeny,
	ShowInbox,
	InboxContent,
	Compose,
	GetMail,
	MailContent,
	GetAttachment,
	AttachmentContent,
	InvalidID
} MessageType;

typedef struct Attachment {
	char* fileName;
	int size;
	unsigned char* data;
} Attachment;

typedef struct Mail {
	short id;
	char* sender;
	char* subject;
	char* body;
	unsigned char numAttachments;
	Attachment* attachments;
} Mail;

typedef struct User {
	char name[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];
	int mailAmount;
	Mail* mails;
} User;

typedef struct Message {
	MessageType messageType;
	int dataSize;
	unsigned char* data;
} Message;

void print_error();

void print_error_message(char *message);

int send_message(int targetSocket, Message *message);

int recv_message(int sourceSocket, Message *message);

int prepare_message_from_string (char *str, Message *message);

int prepare_string_from_message (char **str, Message *message);

int send_empty_message(int socket, MessageType type);

void free_mail_struct(Mail* mail);

void free_message(Message *message);
