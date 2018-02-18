// Modes of Transfer
#define TFTP_OCTET 0
#define TFTP_NETASCII 1

// Max Limits
#define TFTP_MAX_REQ_SIZE 100
#define TFTP_DATA_SIZE 512

// Error Codes
#define TFTP_SOCK_ERROR -1
#define TFTP_FILE_ERROR -2
#define TFTP_TIMEOUT -3


int tftp_send(int, std::string, int, struct sockaddr *);
int tftp_recv(int, std::string, int, struct sockaddr *);
void tftp_err(int, int, std::string, struct sockaddr *);
void tftp_perror(int);