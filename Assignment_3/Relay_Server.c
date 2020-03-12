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
#include <arpa/inet.h>
#include <signal.h> 

#define BUFF_LEN 1024
#define MAX_BACKLOG 100
#define FD_SIZE 100

void handle_sigint(int);

int listen_fd, conn_fd, sock_fd, max_fd, maxi, i, serv_port, nready, client[FD_SIZE], lens, flag;
ssize_t n;
fd_set allset;
char buffer[BUFF_LEN];
socklen_t client_len;
struct sockaddr_in serv_addr, client_addr;

int main(int argc, char **argv) {


    // Creating Peer_Nodes_Info_at_Relay_Server.txt to be filled with Peer_Nodes' information (IP Address and PORT)
	FILE *output, *input;
	output = fopen("Peer_Nodes_Info_at_Relay_Server.txt", "w");
	fclose(output);

	if (argc < 2) {
		fprintf(stderr, "Please Specify Port Number\nUsage %s PORT\n", argv[0]);
		exit(0);
	}

    // Creating a listening socket or printing an unsuccessful error
	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Create Error listen_fd Socket : %d\n", errno);
		exit(EXIT_FAILURE);
	}

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_port = atoi(argv[1]);
	serv_addr.sin_port = htons(serv_port);

    // Binding listening socket or printing an unsuccessful error
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    	perror("setsockopt(SO_REUSEADDR) failed");

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
	for (i = 0; i < FD_SIZE; i++){
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
			printf("Connection Request at Listening Socket...\n");

            // Accepting a connection at listening socket and creating a new connected socket or printing an unsuccessful error
			if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
				printf("Accept Error : %d\n", errno);
				continue;
			}

            // Storing newly created connected socket
			for (i = 0; i < FD_SIZE; i++)	{
				if (client[i] < 0) {
					client[i] = conn_fd;
					break;
				}
			}

			if (i == FD_SIZE)	{
				printf("Relay_Server at Capacity Try Again Later...\n");
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
		sock_fd = conn_fd;
		if (FD_ISSET(sock_fd, &allset)) 	{
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
			printf("Relay_Server Received Message : %s\n", buffer);

			if (strcmp(buffer, "REQUEST : Peer_Node") == 0)
				flag = 1;

			if (strcmp(buffer, "REQUEST : Peer_Client") == 0)
				flag = 2;

			int port = client_addr.sin_port;

			// If the request is by a Peer_Node
			if (flag == 1) {
				printf("Port for Communication with Peer_Node : %d\n", port);

				// Storing the IP address and port number of the Peer_Node or printing an unsuccessful error
				char client_name[INET_ADDRSTRLEN];
				if (inet_ntop (AF_INET, &client_addr.sin_addr.s_addr, client_name, sizeof(client_name)) != NULL) {
					output = fopen("Peer_Nodes_Info_at_Relay_Server.txt", "a+");
					fprintf(output,	"%s%c%d\n",	client_name, ' ', port);
					fclose(output);
				} else {
					printf ("Unable to get Address\n");
				}

				char *resp = "RESPONSE : Peer_Node : 1";
				char buffer[BUFF_LEN];
				sprintf(buffer, "%s %d", resp, port);
				printf ("Relay_Server Sending Message : %s\n", buffer);
				n = write(sock_fd, buffer, strlen(buffer));
				if (n < 0) {
					perror("ERROR Writing to Socket");
					exit(1);
				}
			}

			// If the request is by a Peer_Client
			else if (flag == 2) {
				char *resp = "RESPONSE : Peer_Client : 1";
				n = write(sock_fd, resp, strlen(resp));
				n = read(sock_fd, buffer, BUFF_LEN);
				if (n < 0) {
					perror("ERROR Reading from Socket");
					exit(1);
				}
				printf ("Relay_Server Received Message from Peer_Client : %s\n", buffer);

				if (strcmp (buffer, "REQUEST : Peer_Node Info") == 0) {

					// Reading data from the file storing the information about Peer_Nodes
					FILE *f = fopen("Peer_Nodes_Info_at_Relay_Server.txt", "rb");

					// Finding the length of the file
					fseek(f, 0, SEEK_END);
					long fsize = ftell(f);

					// Set the file pointer to beginning of the file
					fseek(f, 0, SEEK_SET);

					char *resp = malloc(fsize + 1);
					fread(resp, fsize, 1, f);
					fclose(f);

					resp[fsize] = 0;
					printf ("Relay_Server has Peer_Node Information :\n%s", resp);

					// Sending information to the Peer_Client making request or printing an unsuccessful error
					n = write(sock_fd, resp,	strlen(resp));
					if (n < 0) {
						perror("ERROR Writing to Socket");
						exit(1);
					}
				}
			}

			else
				printf("ERROR : Unknown REQUEST Message, Please Check Syntax\n");
			close(sock_fd);
			FD_CLR(sock_fd, &allset);
			client[i] = -1;
			printf("Gracefully Closing Connection\n");

		}
	}

	return 0;
}

void handle_sigint(int sig){
	close(listen_fd);
	FD_CLR(listen_fd, &allset);
	for (i = 0; i < FD_SIZE; i++)	{
		if ((sock_fd = client[i]) > 0)	{
			close(sock_fd);
			FD_CLR(sock_fd, &allset);
			client[i] = -1;
		}
	}
	fflush(stdout);
	// stdout(flush);
	exit(1);
}
