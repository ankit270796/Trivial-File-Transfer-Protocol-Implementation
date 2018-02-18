#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <netdb.h>
#include <sys/wait.h>

#include "tftp.h"

using namespace std;

// Handles child termination
void sigchld_handler(int signo);

int main(int argc, char const *argv[])
{
	int i;
	int sd;
	int err;
	int opcode;
	int bytes_recv;
	char buffer[TFTP_MAX_REQ_SIZE];
	struct addrinfo addr_info, *server_info, *it;
	struct sockaddr_storage client_addr;
	struct sigaction sig;
	socklen_t sockaddr_size;
	string file, mode;

	// Check command line arguments
	if (argc < 2) {
		printf("Argument Error! Try executing:\n");
		printf("./tftps PORT\n");
		return 1;
	}

	memset(&addr_info, 0, sizeof(addr_info));
	addr_info.ai_family = AF_UNSPEC;
	addr_info.ai_socktype = SOCK_DGRAM;
	addr_info.ai_flags = AI_PASSIVE;

	// Register the signal handler for SIGCHLD event
	// so that the resulting process is not a zombie while exiting
	sig.sa_handler = sigchld_handler;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sig, NULL) == -1) {
		perror("sigaction()");
		return 1;
	}

	// Get IP, Port
	if ((err = getaddrinfo(NULL, argv[1], &addr_info, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(err));
		return 1;
	}

	// Iterate through output of getaddrinfo() and find a port to bind to
	for (it = server_info; it != NULL; it = it->ai_next) {
		sd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (sd == -1) {
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("socket()");
			continue;
		}
		if (bind(sd, it->ai_addr, it->ai_addrlen) == -1) {
			close(sd);
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("bind()");
			// If bind was unsuccesssful, try the next port
			// Note: In this case, there happens to be only one port and
			// so the list server_info only has one element
			continue;
		}
		break;
	}

	// Check if server successfully binded to the given port
	if (it == NULL) {
		fprintf(stderr, "Server failed to bind to given port.\n");
		return 1;
	}
	// Free Memory
	freeaddrinfo(server_info);


	// Listen for data !!
	printf("Waiting for clients ...\n");
	sockaddr_size = sizeof(client_addr);

	while (true) {
		bytes_recv = recvfrom(sd, buffer, TFTP_MAX_REQ_SIZE - 1, 0, (struct sockaddr *)&client_addr, &sockaddr_size);
		if (bytes_recv == -1) {
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("recvfrom()");
			return 1;
		}
		buffer[bytes_recv] = '\0';

		// Get OPCODE
		memcpy(&opcode, buffer, 2);
		opcode = htons(opcode);

		// Get Filename and Mode		
		file.clear();
		mode.clear();
		for (i = 2; buffer[i] != 0x00; i++) {
			file += buffer[i];
		}
		for (i++; buffer[i] != 0x00; i++) {
			mode += buffer[i];
		}

		// Create a child process to handle the transfer
		if (!fork()) {
			close(sd);

			// Get an Ephemeral Port ("0")
			if ((err = getaddrinfo(NULL, "0", &addr_info, &server_info)) != 0) {
				fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(err));
				return 1;
			}
			// Iterate through output of getaddrinfo() and find a port to bind to
			for (it = server_info; it != NULL; it = it->ai_next) {
				sd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
				if (sd == -1) {
					printf("%s:%d ", __FILE__, __LINE__);
					fflush(stdout);
					perror("socket()");
					continue;
				}
				if (bind(sd, it->ai_addr, it->ai_addrlen) == -1) {
					close(sd);
					printf("%s:%d ", __FILE__, __LINE__);
					fflush(stdout);
					perror("bind()");
					// If bind was unsuccesssful, try the next port
					// Note: In this case, there happens to be only one port and
					// so the list server_info only has one element
					continue;
				}
				break;
			}
			// Check if server successfully binded to the given port
			if (it == NULL) {
				fprintf(stderr, "Server failed to bind to given port.\n");
				return 1;
			}
			// Free Memory
			freeaddrinfo(server_info);

			// If Opcode == RRQ
			if (opcode == 0x01) {
				printf("Operation: Read %s in %s mode ... ", file.c_str(), mode.c_str());
				fflush(stdout);
				if (mode == "netascii") {
					err = tftp_send(sd, file, TFTP_NETASCII, (struct sockaddr *)&client_addr);
				} else {
					err = tftp_send(sd, file, TFTP_OCTET, (struct sockaddr *)&client_addr);
				}
				if (err != 0) {
					if (err == TFTP_SOCK_ERROR) {
						tftp_err(sd, 0, "Socket Error!", (struct sockaddr *)&client_addr);
					}
					else if (err = TFTP_FILE_ERROR) {
						tftp_err(sd, 1, "File Error!", (struct sockaddr *)&client_addr);
					}
					else if (err = TFTP_TIMEOUT) {
						tftp_err(sd, 0, "Timeout Error!", (struct sockaddr *)&client_addr);
					}
				}
			}
			// If Opcode == WRQ
			else if (opcode == 0x02) {
				printf("Operation: Write %s in %s mode ... ", file.c_str(), mode.c_str());
				fflush(stdout);
				if (mode == "netascii") {
					err = tftp_recv(sd, file, TFTP_NETASCII, (struct sockaddr *)&client_addr);
				} else {
					err = tftp_recv(sd, file, TFTP_OCTET, (struct sockaddr *)&client_addr);
				}
				if (err != 0) {
					if (err == TFTP_SOCK_ERROR) {
						tftp_err(sd, 0, "Socket Error!", (struct sockaddr *)&client_addr);
					}
					else if (err = TFTP_FILE_ERROR) {
						tftp_err(sd, 2, "File Error!", (struct sockaddr *)&client_addr);
					}
					else if (err = TFTP_TIMEOUT) {
						tftp_err(sd, 0, "Timeout Error!", (struct sockaddr *)&client_addr);
					}
				}
			}
			else {
				printf("Unknown Opcode!\n");
				close(sd);
				return 0;
			}
			
			printf("Done\n");
			close(sd);
			return 0;
		}
	}

	close(sd);
	return 0;
}

// SIGCHLD handler
void sigchld_handler(int signo)
{
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}