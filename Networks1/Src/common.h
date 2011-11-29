#define MAX_NAME_LEN 50
#define MAX_PASSWORD_LEN 50
#define MAX_EMAILS 32000

/* Errors */
#define ERROR -1
#define ERROR_SOCKET_CLOSED -2
#define ERROR_LOGICAL -3
#define ERROR_INVALID_ID -4
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

typedef struct Attachment {
	char* fileName;
	int size;
	unsigned char* data;
} Attachment;

typedef struct Mail {
	unsigned short id;
	char* sender;
	char* subject;
	unsigned char numRecipients;
	char** recipients;
	char* body;
	unsigned char numAttachments;
	Attachment* attachments;
	unsigned char numRefrences;
} Mail;

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
	InvalidID,
	DeleteMail,
	DeleteApprove
} MessageType;

typedef enum MessageSize {
	ZeroSize,
	TwoBytes,
	ThreeBytes,
	VariedSize
} MessageSize;

typedef struct Message {
	MessageType messageType;
	MessageSize messageSize;
	int size;
	unsigned char* data;
} Message;

void print_error();

void print_error_message(char *message);

void free_message(Message *message);

int recv_message(int sourceSocket, Message *message);

int send_message_from_string (int socket, char *str);

int recv_string_from_message (int socket, char **str);

int send_message_from_credentials(int socket, char* userName, char* password);

int prepare_credentials_from_message(Message* message, char* userName, char* password);

/* return ERROR on error
 *         1 on accept
 *		   0 on reject */
int recv_credentials_result(int socket);

int send_empty_message(int socket, MessageType type);

void free_attachment(Attachment *attachment);

void free_mail(Mail* mail);

void free_mails(int mailAmount, Mail *mails);

int send_message_from_inbox_content(int socket, Mail **mails, int mailAmount);

int recv_inbox_content_from_message(int socket, Mail **mails, int *mailAmount);

int send_get_mail_message(int socket, unsigned short mailID);

void get_mail_id_from_message(Message *message, unsigned short *mailID, MessageType messageType);

int send_message_from_mail(int socket, Mail *mail);

int recv_mail_from_message(int socket, Mail *mail);

int send_get_attachment_message(int socket, unsigned short mailID, unsigned char attachmentID);

void get_mail_attachment_id_from_message(Message *message, unsigned short *mailID, unsigned char *attachemntID);

int send_message_from_attachment(int socket, Attachment *attachment);

int recv_attachment_from_message(int socket, Attachment *attachment);

int send_delete_mail_message(int socket, unsigned short mailID);

int recv_delete_result(int socket);

FILE* get_valid_file(char* fileName);
