#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <netdb.h>
#include <signal.h>

#include "tftp.h"

using namespace std;

// Opcodes
#define OPCODE_RRQ 0x01
#define OPCODE_WRQ 0x02
#define OPCODE_DAT 0x03
#define OPCODE_ACK 0x04
#define OPCODE_ERR 0x05

#define ACK_SIZE 4

// Number of seconds to wait before timeout
#define TIMEOUT 3
// Max Retries
#define RETRIES 10

// Global Variable used by SIGALRM handler
bool timeout = false;

// Common structure used for data and err
struct data_t {
	unsigned int opcode:16;
	unsigned int block:16;
	unsigned char data[TFTP_DATA_SIZE];
};

struct ack_t {
	unsigned int opcode:16;
	unsigned int block:16;
};

// Alarm (Timeout) handler
void handle_alarm(int signo, siginfo_t *info, void *ptr)
{
	timeout = true;
}

// Send Error Packet
void tftp_err(int sd, int code, std::string msg, struct sockaddr *client)
{
	int i;
	int bytes;
	int payload_size;
	struct data_t err;
	struct sockaddr_storage client_addr;
	socklen_t size;

	size = sizeof(client_addr);
	memset(&err, 0, sizeof(err));
	err.opcode = htons(OPCODE_ERR);
	err.block = htons(code);	// block is same as error code

	// Truncate at 512 bytes
	if (msg. length() > TFTP_DATA_SIZE) {
		payload_size = TFTP_DATA_SIZE + 4;
	} else {
		payload_size = msg.length() + 4;
	}

	for (i = 0; i < (payload_size - 4); i++) {
		err.data[i] = msg[i];
	}

	// Send ERR Packet
	bytes == sendto(sd, &err, payload_size, 0, client, sizeof(struct sockaddr_storage));
	if (bytes == -1) {
		printf("%s:%d ", __FILE__, __LINE__);
		fflush(stdout);
		perror("sendto()");
	}
}

int tftp_recv(int sd, string file, int mode, struct sockaddr *client)
{
	int i;
	int id;
	int bytes;
	int retries;
	char buffer[TFTP_DATA_SIZE + 4];
	struct ack_t ack;
	struct data_t data;
	struct sockaddr_storage client_addr;
	FILE *fp;
	socklen_t size;

	id = 0;
	retries = 0;
	ack.opcode = htons(OPCODE_ACK);
	size = sizeof(client_addr);

	if (mode == TFTP_NETASCII) {
		fp = fopen(file.c_str(), "w");
	} else {
		fp = fopen(file.c_str(), "wb");
	}

	// If file can't be opened
	if (fp == NULL) {
		printf("%s:%d ", __FILE__, __LINE__);
		fflush(stdout);
		perror("fopen()");
		return TFTP_FILE_ERROR;
	}

	// Send ACK0
	ack.block = htons(id++);
	bytes = sendto(sd, &ack, sizeof(ack), 0, client, sizeof(struct sockaddr_storage));
	if (bytes == -1) {
		printf("%s:%d ", __FILE__, __LINE__);
		fflush(stdout);
		perror("sendto()");
		return TFTP_SOCK_ERROR;
	}

	while (1) {
		alarm(TIMEOUT);
		bytes = recvfrom(sd, buffer, TFTP_DATA_SIZE + 4, 0, (struct sockaddr *)&client_addr, &size);
		if (bytes == -1) {
			if (timeout) {
				timeout = false;
				printf("Timeout!\n");
				fflush(stdout);
				retries++;
				if (retries == RETRIES) {
					return TFTP_TIMEOUT;
				}
				continue;
			} else {
				printf("%s:%d ", __FILE__, __LINE__);
				fflush(stdout);
				perror("recvfrom()");
				return TFTP_SOCK_ERROR;
			}
		}
		alarm(0);
		memcpy(&data, buffer, bytes);

		if (data.opcode == htons(OPCODE_ERR)) {
			printf("%s:%d", __FILE__, __LINE__);
			fflush(stdout);
			tftp_perror(ntohs(data.block));
			return TFTP_SOCK_ERROR;
		}
		if (data.opcode != htons(OPCODE_DAT)) {
			printf("%s:%d Corrupt Data", __FILE__, __LINE__);
			fflush(stdout);
			continue;
		}
		// If block id is different, recv again
		if (data.block != htons(id)) {
			continue;
		}
		
		for (i = 0; i < (bytes - 4); i++) {
			fputc(data.data[i], fp);
		}

		// Send ACK
		ack.block = htons(id++);
		bytes = sendto(sd, &ack, sizeof(ack), 0, client, sizeof(struct sockaddr_storage));
		if (bytes == -1) {
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("sendto()");
			return TFTP_SOCK_ERROR;
		}
		retries = 0;

		if (i != 512) {
			break;
		}
	}

	fclose(fp);
	return 0;
}


int tftp_send(int sd, string file, int mode, struct sockaddr *client)
{
	int i;
	int id;
	int bytes;
	int payload_size;
	char ch;
	char buffer[ACK_SIZE];
	struct data_t data;
	struct ack_t ack;
	struct sockaddr_storage client_addr;
	struct sigaction act;
	FILE *fp;
	socklen_t size;

	data.opcode = htons(OPCODE_DAT);
	size = sizeof(client_addr);
	id = 1;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = handle_alarm;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGALRM, &act, NULL);

	// Open the file on the server
	if (mode == TFTP_NETASCII) {
		fp = fopen(file.c_str(), "r");
	} else {
		fp = fopen(file.c_str(), "rb");
	}

	// Check if file can't be opened
	if (fp == NULL) {
		printf("%s:%d ", __FILE__, __LINE__);
		fflush(stdout);
		perror("fopen()");
		return TFTP_FILE_ERROR;
	}

	while(1) {
		data.block = htons(id++);
		// Fill one block of data
		if (mode == TFTP_NETASCII) {
			for (i = 0; i < TFTP_DATA_SIZE; i++) {
				ch = fgetc(fp);
				if (ch == EOF) {
					break;
				}
				data.data[i] = ch;
			}
			payload_size = i + 4;
		} else {
			bytes = fread(&data.data, sizeof(unsigned char), TFTP_DATA_SIZE, fp);
			if (bytes != TFTP_DATA_SIZE) {
				ch = EOF;
			}
			payload_size = bytes + 4;
		}

		// Try to send until we receive a proper ACK
		// or until we exceed the maximum retries
		i = RETRIES;
		while(i > 0) {
			bytes = sendto(sd, &data, payload_size, 0, client, sizeof(struct sockaddr_storage));
			if (bytes == -1) {
				printf("%s:%d ", __FILE__, __LINE__);
				fflush(stdout);
				perror("sendto()");
				return TFTP_SOCK_ERROR;
			}

			alarm(TIMEOUT);
			bytes = recvfrom(sd, buffer, ACK_SIZE, 0, (struct sockaddr *)&client_addr, &size);
			if (bytes == -1) {
				if (timeout) {
					printf("Timeout!\n");
					fflush(stdout);
					timeout = false;
					i--;
					continue;
				} else {
					printf("%s:%d ", __FILE__, __LINE__);
					fflush(stdout);
					perror("recvfrom()");
					return TFTP_SOCK_ERROR;
				}
			}
			alarm(0);
			
			memcpy(&ack, buffer, ACK_SIZE);
			if (ack.opcode == htons(OPCODE_ERR)) {
				printf("%s:%d", __FILE__, __LINE__);
				fflush(stdout);
				// ACK BlockID is same as ErrorCode for ERR
				tftp_perror(ntohs(ack.block));
				return TFTP_SOCK_ERROR;
			}
			if (ack.opcode != htons(OPCODE_ACK)) {
				printf("%s:%d Corrupt Data", __FILE__, __LINE__);
				fflush(stdout);
				continue;
			}

			if (ack.block == htons(id-1)) {
				break;
			}
			i--;
		}
		if (i == 0) {
			printf("%s:%d Timeout Error\n", __FILE__, __LINE__);
			fflush(stdout);
			return TFTP_TIMEOUT;
		}

		// If reached end of file, terminate while(1) loop
		if (ch == EOF) {
			break;
		}
	}

	fclose(fp);
	return 0;
}

void tftp_perror(int err)
{
	switch(err) {
		case 1 : printf("File not found.\n"); break;
		case 2 : printf("Access violation.\n"); break;
		case 3 : printf("Disk full or allocation exceeded.\n"); break;
		case 4 : printf("Illegal TFTP operation.\n"); break;
		case 5 : printf("Unknown transfer ID.\n"); break;
		case 6 : printf("File already exists.\n"); break;
		case 7 : printf("No such user.\n"); break;
		default: printf("Error Code not defined, see error message.\n"); break;
	}
	fflush(stdout);
}