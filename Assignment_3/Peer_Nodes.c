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
#include <signal.h>

#define BUFF_LEN 1024
#define MAX_BACKLOG 100
#define FD_SIZE 100

int start_server(char *);
int connecting_to_relay_server();
void handle_sigint(int);

struct hostent *server;
int relay_server_port;

int main(int argc, char **argv)
{
	int sock_fd;
	char buffer[BUFF_LEN];

	if (argc < 3) {
		fprintf(stderr, "Please Specify Relay_Server's IP Address and Port Number\nUSAGE : %s IP_Address Port_Number\n", argv[0]);
		exit(0);
	}
	server = gethostbyname(argv[1]);
	relay_server_port = atoi(argv[2]);
	sock_fd = connecting_to_relay_server();

	// Informing the Relay_Server that the request is by Peer_Node or printing unsuccessful error
	char *request = "REQUEST : Peer_Node";

	if (write(sock_fd, request, strlen(request)) < 0) {
		printf("ERROR : Cannot Write to Relay_Server");
		exit(EXIT_FAILURE);
	}

	bzero(buffer, BUFF_LEN);

	// Reading Relay_Server response or printing unsuccessful error
	if (read(sock_fd, buffer, BUFF_LEN) < 0) {
		printf("ERROR : Cannot Read from Relay_Server");
		exit(EXIT_FAILURE);
	}

	printf("Peer_Node Received : \n%s\n", buffer);

	if (buffer[23] == '1') {
		printf("RESPONSE : Peer_Node Accepted by Relay_Server\nConnected Successfully\n");

		// Relay_Server takes the requisite action (saves IP Address and PORT number)
		// Close the connection with Relay_Server gracefully
		printf("Gracefully Closing Connection...\n");

		if (close(sock_fd) < 0) {
			printf("ERROR : Cannot Close Connection with Relay_Server\n");
			exit(EXIT_FAILURE);
		}

		// Starting server behaviour of Peer_Node if successfully connected to server
		printf("Port of the Peer_Node to be used for Listening : %s\n",&buffer[25]);
		start_server(&buffer[25]);
	} else
		printf("ERROR : Peer_Node Rejected by Relay_Server\nExiting...\n");

	return 0;

}

int listen_fd, client[FD_SIZE];
fd_set allset;
int serv_port;

int start_server(char *port){
	int conn_fd, sock_fd, max_fd, maxi, i, nready;
	char buffer[BUFF_LEN];
	socklen_t client_len;
	ssize_t n;
	struct sockaddr_in serv_addr, client_addr;
	serv_port = atoi(port);
	char cur_port[10];

	strcpy(cur_port,port);

	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("ERROR : Cannot Create listen_fd Socket : %d\n", errno);
		exit(EXIT_FAILURE);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(serv_port);

	// Binding listening socket or printing an unsuccessful error
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    	printf("ERROR : 'setsockopt(SO_REUSEADDR)' Failed\n");

	if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
		printf("ERROR : Cannot Bind listen_fd Socket : %d\n", errno);
		exit(EXIT_FAILURE);
	}

	// Listening at listening socket or printing an unsuccessful error
	if (listen(listen_fd, MAX_BACKLOG) == -1) {
		printf("ERROR : Cannot Listen at listen_fd Socket : %d\n", errno);
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
	signal(SIGINT, handle_sigint);

	while (1) {

		// Checking the first max_fd descriptors from fd_set allset to see if they ready for reading  or printing an unsuccessful error
		if ((nready = select(max_fd + 1, &allset, NULL, NULL, NULL)) == -1) {
			printf("ERROR : Select : %d\n", errno);
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
				printf("ERROR : Accept : %d\n", errno);
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
					if (n <= 0) {
						if(n<0)
							printf("ERROR : Cannot Read at sock_fd %d\n",sock_fd);
						if(n==0)
							printf("WARNING : No Data at sock_fd %d\n",sock_fd);

						close(sock_fd);
						FD_CLR(sock_fd, &allset);
						client[i] = -1;
						continue;
					}

					printf("Peer_Node Received : %s\n", buffer);
					char req_format[] = "REQUEST : FILE : ";
					int i, req_type = 0;
					for (i = 0; i < strlen(req_format); i++) {
						if (req_format[i] == buffer[i])
							req_type = 1;
						else {
							req_type = 0;
							break;
						}
					}
					if (req_type){
						printf("Peer_Node Received Request for File : %s\n", &buffer[strlen(req_format)]);
						FILE *file = fopen(&buffer[strlen(req_format)], "r");
						if (file == NULL) {
							printf("Requested File NOT Found at Peer_Node\n");
							char response[] = "File NOT FOUND";
							n = write(sock_fd, response, strlen(response));
							if (n < 0)
								printf("ERROR : Cannot Write to Peer_Client");
							else
								printf("Requested File NOT Found and written to Peer_Client\n");
						}

						// If file found
						else {
							printf("Requested File Found at Peer_Node\n");
							char response[] = "File FOUND";
							n = write(sock_fd, response, strlen(response));
							if (n < 0){
								printf("ERROR : Cannot Write to Peer_Client");
								exit(EXIT_FAILURE);
							}
							else
								printf("Requested File Found and written to Peer_Client\n");

							// Sending the file to the client

							// Finding the length of the file
							fseek(file, 0, SEEK_END);
							long fsize = ftell(file);

							// Set the file pointer to beginning of the file
							fseek(file, 0, SEEK_SET);

							char *resp = malloc(fsize + 1);
							fread(resp, fsize, 1, file);
							fclose(file);
							printf("Printing File Content... \n%s", resp);

							// Sending the requested content to client
							n = write(sock_fd, resp, strlen(resp));
							if (n < 0) {
								printf("ERROR : Cannot Write File Content to Peer_Client");
								exit(EXIT_FAILURE);
							}
							else {
								printf("File Sent Successfully\nGracefully Closing Connection with Peer_Client...\n");
							}
						}
					}
					else{
						printf("ERROR : Unexpected Peer_Client Request DUMPING...\n");
					}
					close(sock_fd);
					FD_CLR(sock_fd, &allset);
					client[i] = -1;
				}
			}
		}
	}
}

int connecting_to_relay_server(){
		// Creating a socket or printing unsuccessful error
	int sock_fd;
	struct sockaddr_in serv_addr;
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("ERROR : Cannot Create sock_fd Socket : %d\n", errno);
		exit(EXIT_FAILURE);
	}
	if (server == NULL) {
		fprintf(stderr, "ERROR : Host Not Found\n");
		exit(EXIT_FAILURE);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(relay_server_port);

    // Connecting to the Relay_Server or printing unsuccessful error
	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR : Cannot Connect to Relay_Server\n");
		exit(EXIT_FAILURE);
	}

	printf("Connection attempt with Relay_Server Successful\nSending Request...\n");
	return sock_fd;
}


void handle_sigint(int sig){
	close(listen_fd);
	FD_CLR(listen_fd, &allset);
	int sock_fd, i;

	for (i = 0; i < FD_SIZE; i++)	{
		if ((sock_fd = client[i]) > 0)	{
			close(sock_fd);
			FD_CLR(sock_fd, &allset);
			client[i] = -1;
		}
	}
	// Informing the Relay_Server that the request Peer_Node is exiting or printing unsuccessful error
	char request[100];
	sprintf(request,"REQUEST : Peer_Exit ");
	printf("%s\n", request);
	sock_fd = connecting_to_relay_server();
	char Port_String[10];
	sprintf(Port_String,"%d",serv_port);
	strcat(request, Port_String);
	if (write(sock_fd, request, strlen(request)) < 0) {
		printf("ERROR : Cannot Write to Relay_Server");
		exit(EXIT_FAILURE);
	}
	printf("Relay_Server Notified of Peer_Node Closure\n");


	fflush(stdout);
	exit(EXIT_FAILURE);
}
