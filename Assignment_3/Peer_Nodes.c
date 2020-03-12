#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUFF_LEN 1024
#define MAX_BACKLOG 100
#define FD_SIZE 100

int start_server(char *port);

int main(int argc, char **argv)
{
	int listen_fd, conn_fd, sock_fd, max_fd, maxi, i, nready, client[FD_SIZE], lens, serv_port;
	ssize_t n;
	fd_set allset;
	char buffer[BUFF_LEN];
	socklen_t client_len;
	struct sockaddr_in serv_addr, client_addr;
	struct hostent *server;

	if (argc < 3) {
		fprintf(stderr, "Please Specify IP Address and Port Number\nUsage %s IP_Address PORT\n", argv[0]);
		exit(0);
	}

	// Creating a socket
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	serv_port = atoi(argv[2]);
	server = gethostbyname(argv[1]);

	if (server == NULL) {
		fprintf(stderr, "ERROR : No Such Host\n");
		exit(0);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(serv_port);

    // Connecting to the Relay_Server
	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR connecting");
		exit(1);
	}

	printf("Connected to the Relay_Server...\n Sending Request message...\n");
	char *req = "REQUEST : Peer_Node";

	// Sending request for Peer_Node information to the Relay_Server
	n = write(sock_fd, req, strlen(req));

	if (n < 0) {
		perror("ERROR Writing to Socket");
		exit(1);
	}

	// Reading Relay_Server response
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN);

	if (n < 0) {
		perror("ERROR Reading from Socket");
		exit(1);
	}

	printf("Peer_Node Received : \n%s\n", buffer);

	if (buffer[23] == '1') {
		printf("RESPONSE : Peer_Node Accepted\nSUCESSFULLY Connected\n");
		// Close the connection with Relay_Server gracefully
		printf("Gracefully Closing Connection ...\n");
		n = shutdown(sock_fd, 0);
		if (n < 0) {
			perror("ERROR Closing Connection");
			exit(1);
		}

		// Starting server behaviour of Peer_Node if successfully connected to server
		// printf("Port of the Peer_Client for Listening : %s\n",&buffer[23]);
		// printf("Port of the Peer_Client for Listening : %s\n",&buffer[24]);
		printf("Port of the Peer_Client for Listening : %s\n",&buffer[25]);

		start_server(&buffer[25]);
	} else
		printf("Node NOT Accepted by the Relay_Server\nTry Again...\n");

	return 0;

}

int start_server(char *port){
	int listen_fd, conn_fd, sock_fd, max_fd, maxi, i, nready, client[FD_SIZE],lens;
	ssize_t n;
	fd_set allset;
	char buffer[BUFF_LEN];
	socklen_t client_len;
	struct sockaddr_in serv_addr, client_addr;
	int serv_port = atoi(port);
	// printf("%d\n%s\n", serv_port, port);

	// Creating a listening socket or printing an unsuccessful error
	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Create Error listen_fd Socket : %d\n", errno);
		exit(EXIT_FAILURE);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(serv_port);

	// Binding listening socket or printing an unsuccessful error
	if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
		printf("Bind Error listen_fd Socket : %d\n", errno);
		exit(EXIT_FAILURE);
	}

	// Listening at listening socket or printing an unsuccessful error
	if (listen(listen_fd, MAX_BACKLOG) == -1) {
		printf("Listen Error listen_fd Socket : %d\n", errno);
		exit(EXIT_FAILURE);
	}

	max_fd = listen_fd;
	maxi = -1;

	// Initializing an array of fds
	for (i = 0; i < FD_SIZE; i++)	{
		client[i] = -1;
	}

	// Clearing fd_set allset
	FD_ZERO(&allset);

	// Adding listen_fd to fd_set allset
	FD_SET(listen_fd, &allset);

	while (1) {

		// Checking the first max_fd descriptors from fd_set allset to see if they ready for reading  or printing an unsuccessful error
		if ((nready = select(max_fd + 1, &allset, NULL, NULL, NULL)) == -1) {
			printf("Select Error : %d\n", errno);
			exit(EXIT_FAILURE);
		}
		//If no one is ready to be read; continue
		if (nready <= 0){
			continue;
		}

		// Activity at listening socket (indicating request for new connection)
		if (FD_ISSET(listen_fd, &allset)){
			client_len = sizeof(client_addr);

			// Accepting a connection at listening socket and creating a new connected socket or printing an unsuccessful error
			if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
				printf("Accept Error : %d\n", errno);
				continue;
			}

			// Storing newly created connected socket
			for (i = 0; i < FD_SIZE; i++)	{
				if (client[i] < 0){
					client[i] = conn_fd;
					break;
				}
			}

			if (i == FD_SIZE)	{
				printf("Peer_Node at Capacity Try Again Later...\n");
				close(conn_fd);
				continue;
			}

			// Adding newly created connected socket to the fd_set allset
			FD_SET(conn_fd, &allset);

			if (conn_fd > max_fd)	{
				max_fd = conn_fd;
			}

			if (i > maxi)	{
				maxi = i;
			}
		}

		for (i = 0; i <= maxi; i++)	{
			if ((sock_fd = client[i]) > 0)	{
				if (FD_ISSET(sock_fd, &allset)) {
					memset(buffer, 0, sizeof(buffer));

					n = read(sock_fd, buffer, BUFF_LEN);
					if (n < 0) {
						printf("Error Reading at sock_fd %d\n",sock_fd);
						close(sock_fd);
						FD_CLR(sock_fd, &allset);
						client[i] = -1;
						continue;
					}
					if (n == 0) {
						printf("No Data at sock_fd %d\n",sock_fd);
						close(sock_fd);
						FD_CLR(sock_fd, &allset);
						client[i] = -1;
						continue;
					}

					printf("Peer_Node Received : %s\n", buffer);
					char a[] = "REQUEST : FILE : ";
					int i, flag = 0;
					for (i = 0; i < strlen(a); i++) {
						if (a[i] == buffer[i])
							flag = 1;
						else {
							flag = 0;
							break;
						}
					}
					if (flag){
						printf("Received Request for the File : %s\n", &buffer[strlen(a)]);
						FILE *file = fopen(&buffer[strlen(a)], "r");

						//If file NOT found
						if (file == NULL) {
							printf("Requested File NOT Found\n");
							char response[] = "File NOT FOUND";
							n = write(sock_fd, response, strlen(response));
							if (n < 0)
								perror("ERROR Writing to Socket");
						}

						// If file found
						else {
							printf("Requested File Found\n");
							char response[] = "File FOUND";
							n = write(sock_fd, response, strlen(response));
							if (n < 0)
								perror("ERROR Writing to Socket");

							// Sending the file to the client

							// Finding the length of the file
							fseek(file, 0, SEEK_END);
							long fsize = ftell(file);

							// Set the file pointer to beginning of the file
							fseek(file, 0, SEEK_SET);

							char *resp = malloc(fsize + 1);
							fread(resp, fsize, 1, file);
							fclose(file);
							printf("File has the following content : \n%s", resp);

							// Sending the requested content to client
							n = write(sock_fd, resp, strlen(resp));
							if (n < 0) {
								perror("ERROR Writing to Socket");
								exit(1);
							}
							else {
								printf("File Sent\n Gracefully Closing Connection...\n");
							}
							close(sock_fd);
							FD_CLR(sock_fd, &allset);
							client[i] = -1;
						}
					}
					else
						printf("ERROR : Unknown REQUEST Message, Please Check Syntax\n");
				}
			}
		}
	}
}
