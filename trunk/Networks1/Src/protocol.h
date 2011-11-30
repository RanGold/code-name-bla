#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "common.h"

typedef struct {
	char* fileName;
	int size;
	unsigned char* data;
} Attachment;

typedef struct {
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

typedef enum {
	String,
	Quit,
	Credentials,
	CredentialsAccept,
	CredentialsDeny,
	ShowInbox,
	InboxContent,
	GetMail,
	MailContent,
	GetAttachment,
	AttachmentContent,
	InvalidID,
	DeleteMail,
	DeleteApprove,
	Compose,
	SendApprove,
	InvalidCommand
} MessageType;

typedef enum {
	ZeroSize,
	TwoBytes,
	ThreeBytes,
	VariedSize
} MessageSize;

typedef struct {
	MessageType messageType;
	MessageSize messageSize;
	int size;
	unsigned char *data;
} Message;

void free_message(Message *message);

int recv_message(int sourceSocket, Message *message);

int send_message_from_string (int socket, char *str);

int recv_string_from_message (int socket, char **str);

int send_message_from_credentials(int socket, char* userName, char* password);

int prepare_credentials_from_message(Message* message, char* userName, char* password);

int recv_credentials_result(int socket, int *isLoggedIn);

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

void get_mail_attachment_id_from_message(Message *message, unsigned short *mailID, unsigned char *attachmentID);

int send_message_from_attachment(int socket, Attachment *attachment);

int recv_attachment_from_message(int socket, Attachment *attachment);

int send_delete_mail_message(int socket, unsigned short mailID);

int recv_delete_result(int socket);

int send_compose_message_from_mail(int socket, Mail *mail);
