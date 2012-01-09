#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include "common.h"

/* This struct is for the client and server to represent an attachment data structure */
typedef struct {
	char* fileName;
	int size;
	unsigned char* data;
	FILE* file;
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
	InvalidCommand,
	String,
	Quit,
	CredentialsMain,
	CredentialsChat,
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
	ShowOnlineUsers,
	OnlineUsers,
	ChatMessageSend,
	ChatMessageReceive,
	ChatMailConfirm,
	ChatConfirm
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

typedef struct {
	Message message;
	int dataOffset;
	int isPartial;
	int headerHandled;
	int sizeHandled;
	int messageInitialized;
} NonBlockingMessage;

typedef int(*InteruptFunction)(int);

/* Frees the message struct */
void free_message(Message *message);

/* Frees the NonBlockingMessage struct */
void free_non_blocking_message(NonBlockingMessage *userBuffer);

/* Send a message to the stream */
int send_message(int targetSocket, Message *message, int interuptSocket, InteruptFunction interuptFunction);

/* Receive a message from the stream */
int recv_message(int sourceSocket, Message *message, int interuptSocket, InteruptFunction interuptFunction);

/* Send a non blocking message to the stream with attachments */
int send_non_blocking_message_with_attachments(int targetSocket, NonBlockingMessage *nbMessage, Mail *mail);

/* Send a non blocking message to the stream */
int send_non_blocking_message(int targetSocket, NonBlockingMessage *nbMessage);

/* Receive a non blocking message from the stream */
int recv_non_blocking_message(int sourceSocket, NonBlockingMessage *nbMessage);

/* Returns if a full message was received */
int is_full_message_received(NonBlockingMessage *nbMessage);

/* Returns if there is a message to send */
int is_there_message_to_send(NonBlockingMessage *nbMessage);

/* prepares a message from string */
int prepare_message_from_string(char* str, NonBlockingMessage *nbMessage);

/* Receive a string from a message */
int recv_string_from_message(int socket, char **str, int interuptSocket, InteruptFunction interuptFunction);

/* Send a quit message */
int send_quit_message(int socket);

/* Send a message containing credentials data */
int send_message_from_credentials(int socket, int chatSocket, char* userName, char* password);

/* Preparing credentials data from a received message */
int prepare_credentials_from_message(Message* message, char* userName, char* password);

/* prepares deny answer on the credentials sent */
void prepare_credentials_deny_message(NonBlockingMessage* nbMessage);

/* prepares accept answer on the credentials sent */
void prepare_credentials_approve_message(NonBlockingMessage* nbMessage);

/* Receive credentials approve or deny only */
int recv_credentials_result(int socket, int *isLoggedIn, int interuptSocket, InteruptFunction interuptFunction);

/* Frees attachment struct */
void free_attachment(Attachment *attachment);

/* Frees mail struct including the contained structs */
void free_mail(Mail* mail);

/* Frees a mail array struct including the contained structs */
void free_mails(int mailAmount, Mail *mails);

/* Send a show inbox message */
int send_show_inbox_message(int socket, int interuptSocket, InteruptFunction interuptFunction);

/* Send a message containing the data need to display the inbox content */
int prepare_message_from_inbox_content(Mail **mails, unsigned short mailAmount, NonBlockingMessage* nbMessage);

/* Receive inbox content data from a message */
int recv_inbox_content_from_message(int socket, Mail **mails, unsigned short *mailAmount,
									int interuptSocket, InteruptFunction interuptFunction);

/* Send get mail by id message */
int send_get_mail_message(int socket, unsigned short mailID, int interuptSocket, InteruptFunction interuptFunction);

/* Preparing mail id from a received message */
unsigned short prepare_mail_id_from_message(NonBlockingMessage *nbMessage, MessageType messageType);

/* Prepares invalid id message */
void prepare_invalid_id_message(NonBlockingMessage *nbMessage);

/* Prepares a message containing mail data not including its attachments */
int prepare_message_from_mail(Mail *mail, NonBlockingMessage *nbMessage, int isChat);

/* Receive a message containing mail data not including its attachments */
int recv_mail_from_message(int socket, Mail *mail, int interuptSocket, InteruptFunction interuptFunction);

/* Send get attachment by mail and attachment id message */
int send_get_attachment_message(int socket, unsigned short mailID, unsigned char attachmentID,
								int interuptSocket, InteruptFunction interuptFunction);

/* Preparing attachment id and mail if from a received message */
void prepare_mail_attachment_id_from_message(NonBlockingMessage *nbMessage, unsigned short *mailID, unsigned char *attachmentID);

/* Prepares message containing an attachment data */
int prepare_message_from_attachment(Attachment *attachment, NonBlockingMessage *nbMessage);

/* Receive an attachment data from message */
int recv_attachment_file_from_message(int socket, char* attachmentPath,
									  int interuptSocket, InteruptFunction interuptFunction);

/* Send delete mail by id message */
int send_delete_mail_message(int socket, unsigned short mailID, int interuptSocket, InteruptFunction interuptFunction);

/* Prepares delete approve message */
void prepare_delete_approve_message(NonBlockingMessage *nbMessage);

/* Receiving delete mail by id result */
int recv_delete_result(int socket, int interuptSocket, InteruptFunction interuptFunction);

/* Send compose message from a mail, including all the attachments */
int send_compose_message_from_mail(int socket, Mail *mail, int interuptSocket, InteruptFunction interuptFunction);

/* Sends a chat message from a mail, ignoring non relevant fields */
int send_chat_from_mail(int socket, Mail *mail, int interuptSocket, InteruptFunction interuptFunction);

/* Receiving a chat message */
int recv_chat_from_message(int socket, Mail *ChatMessage);

/* Prepares a message for approving a mail was sent instead of a chat message */
void prepare_chat_mail_confirm_message(NonBlockingMessage *nbMessage);

/* Prepares a message for approving a chat message sent */
void prepare_chat_confirm_message(NonBlockingMessage *nbMessage);

/* Prepare mail data including attachments a compose message */
int prepare_mail_from_compose_message(NonBlockingMessage *nbMessage, Mail **mail);

/* Receive chat message result */
int recv_chat_result(int socket, int *isMailSent, int interuptSocket, InteruptFunction interuptFunction);

/* Prepares send approve message */
void prepare_send_approve_message(NonBlockingMessage *nbMessage);

/* Receive compose result */
int recv_send_result(int socket, int interuptSocket, InteruptFunction interuptFunction);

/* Prepares invalid command message */
void prepare_invalid_command_message(NonBlockingMessage *nbMessage);

/* Prepares a message containing online users names */
int prepare_online_users_message(NonBlockingMessage *nbMessage, char **onlineUsersNames, int usersAmount);

/* Send a show online message */
int send_show_online_users(int socket, int interuptSocket, InteruptFunction interuptFunction);

/* Receive online users names */
int recv_online_users(int socket, char*** onlineUsersNames, int *usersAmount, int interuptSocket, InteruptFunction interuptFunction);

/* Frees the online users array content */
void free_online_users_names(char **onlineUsersNames);
