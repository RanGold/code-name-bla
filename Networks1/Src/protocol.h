#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "common.h"

/* This struct is for the client and server to represent an attachment data structure */
typedef struct {
	char* fileName;
	int size;
	unsigned char* data;
} Attachment;

/* This struct is for the client and server to represent a mail data structure */
typedef struct {
	unsigned short clientId;
	char* sender;
	char* subject;
	unsigned char numRecipients;
	char** recipients;
	char* body;
	unsigned char numAttachments;
	Attachment* attachments;
	unsigned char numRefrences;
} Mail;

/* This enum is to represent the different kinds of messages sent on the protocol */
typedef enum {
	String,
	Quit,
	Credentials,
	CredentialsApprove,
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

/* This enum is to represent common message size's */
typedef enum {
	ZeroSize,
	TwoBytes,
	ThreeBytes,
	VariedSize
} MessageSize;

/* This struct's data is the only one being transfered via the protocol */
typedef struct {
	MessageType messageType;
	MessageSize messageSize;
	int size;
	unsigned char *data;
} Message;

/* Frees the message struct */
void free_message(Message *message);

/* Receive a message from the stream */
int recv_message(int sourceSocket, Message *message);

/* Send a message from a string */
int send_message_from_string (int socket, char *str);

/* Receive a string from a message */
int recv_string_from_message (int socket, char **str);

/* Send a quit message */
int send_quit_message(int socket);

/* Send a message containing credentials data */
int send_message_from_credentials(int socket, char* userName, char* password);

/* Preparing credentials data from a received message */
int prepare_credentials_from_message(Message* message, char* userName, char* password);

/* Send deny answer on the credentials sent */
int send_credentials_deny_message(int socket);

/* Send accept answer on the credentials sent */
int send_credentials_approve_message(int socket);

/* Receive credentials approve or deny only */
int recv_credentials_result(int socket, int *isLoggedIn);

/* Frees attachment struct */
void free_attachment(Attachment *attachment);

/* Frees mail struct including the contained structs */
void free_mail(Mail* mail);

/* Frees a mail array struct including the contained structs */
void free_mails(int mailAmount, Mail *mails);

/* Send a show inbox message */
int send_show_inbox_message(int socket);

/* Send a message containing the data need to display the inbox content */
int send_message_from_inbox_content(int socket, Mail **mails, unsigned short mailAmount);

/* Receive inbox content data from a message */
int recv_inbox_content_from_message(int socket, Mail **mails, unsigned short *mailAmount);

/* Send get mail by id message */
int send_get_mail_message(int socket, unsigned short mailID);

/* Preparing mail id from a received message */
void prepare_mail_id_from_message(Message *message, unsigned short *mailID, MessageType messageType);

/* Send invalid id message */
int send_invalid_id_message(int socket);

/* Send a message containing mail data not including its attachments */
int send_message_from_mail(int socket, Mail *mail);

/* Receive a message containing mail data not including its attachments */
int recv_mail_from_message(int socket, Mail *mail);

/* Send get attachment by mail and attachment id message */
int send_get_attachment_message(int socket, unsigned short mailID, unsigned char attachmentID);

/* Preparing attachment id and mail if from a received message */
void prepare_mail_attachment_id_from_message(Message *message, unsigned short *mailID, unsigned char *attachmentID);

/* Send message containing an attachment data */
int send_message_from_attachment(int socket, Attachment *attachment);

/* Receive an attachment data from message */
int recv_attachment_from_message(int socket, Attachment *attachment);

/* Send delete mail by id message */
int send_delete_mail_message(int socket, unsigned short mailID);

/* Send delete approve message */
int send_delete_approve_message(int socket);

/* Receiving delete mail by id result */
int recv_delete_result(int socket);

/* Send compose message from a mail, including all the attachments */
int send_compose_message_from_mail(int socket, Mail *mail);

/* Prepare mail data including attachments a compose message */
int prepare_mail_from_compose_message(Message *message, Mail **mail);

/* Send send approve message */
int send_send_approve_message(int socket);

/* Receive compose result */
int recv_send_result(int socket);

/* Send invalid command message */
int send_invalid_command_message(int socket);
