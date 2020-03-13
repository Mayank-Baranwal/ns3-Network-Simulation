#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define BUFF_LEN 1024

int find_peer_list(int sock_fd);
int peer_connect(char *address, int port, char *filename);
void handle_sigint(int);

int sock_fd, serv_port, n;
struct sockaddr_in serv_addr;
struct hostent *server;
char buffer[BUFF_LEN];

int main(int argc, char **argv){

	if (argc < 3) {
		fprintf(stderr, "Please Specify Relay_Server's IP Address and Port Number\nUSAGE : %s IP_Address Port_Number\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	server = gethostbyname(argv[1]);
	serv_port = atoi(argv[2]);

	// Creating a socket or printing unsuccessful error
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("ERROR : Cannot Create sock_fd Socket\n");
		exit(EXIT_FAILURE);

	}
	if (server == NULL) {
		fprintf(stderr, "ERROR : Host Not Found\n");
		exit(EXIT_FAILURE);
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(serv_port);

	// Connecting to Relay_Server or printing unsuccessful error
	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "ERROR : Cannot Connect to Relay_Server\n");
		exit(EXIT_FAILURE);
	}

	printf("Connection attempt with Relay_Server Successful\nSending Request...\n");

	// Informing the Relay_Server that the request is by Peer_Client or printing unsuccessful error
	char request[BUFF_LEN] = "REQUEST : Peer_Client";

	if (write(sock_fd, request, strlen(request)) < 0) {
		printf("ERROR : Cannot Write to Relay_Server\n");
		exit(EXIT_FAILURE);
	}

	bzero(buffer, BUFF_LEN);

	// Reading Relay_Server response or printing unsuccessful error
	if (read(sock_fd, buffer, BUFF_LEN-1) < 0) {
		printf("ERROR : Cannot Read from Relay_Server");
		exit(EXIT_FAILURE);
	}

	printf("Peer_Client Received : %s\n", buffer);

	if (buffer[25] == '1') {
		printf("RESPONSE : Peer_Client Accepted by Relay_Server\nConnected Successfully\nFetching Peer_Node Information...\n");
		n = find_peer_list(sock_fd);

		if (n < 0) {
			printf("ERROR : Cannot Retrieve Peer_Nodes from Relay_Server");
			exit(EXIT_FAILURE);
		}
	}
   else
		printf("ERROR : Peer_Client Rejected by the Relay_Server\nExiting...\n");

	return 0;
}

int find_peer_list(int sock_fd)
{
	// Requesting Active Peer_Node Information
	char *request = "REQUEST : Peer_Node Info", buffer[BUFF_LEN];

	// Sending message to Relay_Server
	if (write(sock_fd, request, strlen(request)) < 0) {
		printf("ERROR : Cannot Write to Relay_Server (Peer_Node Info stage)\n");
		exit(EXIT_FAILURE);
	}

	// Reading Relay_Server response
	bzero(buffer, BUFF_LEN);
	if (read(sock_fd, buffer, BUFF_LEN-1) < 0) {
		printf("ERROR : Cannot Read from Relay_Server (Peer_Node Info stage)\n");
		exit(EXIT_FAILURE);
	}
	printf("Peer_Client Received (Peer_Node Info) : \n%s", buffer);
	printf("Gracefully Closing Connection with Relay_Server....\n\n");
	if (close(sock_fd) < 0) {
		printf("ERROR : Cannot Close Connection with Relay_Server (Peer_Node Info stage)\n");
		exit(EXIT_FAILURE);
	}


	char file[50];
	printf("Enter the name of the File that you want : ");
	scanf("%s", file);

	// Trying to retrieve file from Peer_Nodes (one at a time)
	char peer_name[INET_ADDRSTRLEN];
	int port, flag = 0;
	char port_array[10];
	int b=0;
	while (buffer[b]!='\0') {
		int pn=0;
		while(buffer[b]!=' '){
			peer_name[pn]=buffer[b];
			pn++;
			b++;
		}
		peer_name[pn] = '\0';
		pn=0;
		b++;
		while(buffer[b]!='\n'){
			port_array[pn] = buffer[b];
			pn++;
			b++;
		}
		port_array[pn] = '\0';
		port = atoi(port_array);
		b++;
		// peer_connect() connects client to each Peer_Node in the list by Relay_Server and check for requested file
		n = peer_connect(peer_name, port, file);
		printf("\n");
		
		// printf("%s %d %d\n", peer_name, port, n);
		if (n < 0)
			continue;
		else {
			flag = 1;
			break;
		}
	}

	if (!flag)
		printf("WARNING : Sorry! File Not Found on ANY Peer_Node given by the Relay_Server\n");

	return 0;
}

int peer_connect(char *address, int port, char *filename){
	int sock_fd, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	struct in_addr ipv4addr;
	char buffer[BUFF_LEN];

	// Creating a socket or printing unsuccessful error
	// printf("%s %d\n", address, port);

	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("ERROR : Socket Creation Failed\n");
		exit(EXIT_FAILURE);
	}

	// inet_pton(AF_INET, address, &ipv4addr);
	// server = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
	server = gethostbyname(address);


	if (server == NULL) {
		fprintf(stderr, "ERROR : Host Not Found\n");
		exit(0);
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(port);

	// Connecting to Peer_Node (specified by input fields) or printing an unsuccessful error
	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR : Cannot Connect to Peer_Node with Address %s and PORT %d\n",address,port);
		return -1;
	}

	printf ("Connected to Peer_Node: %s %d\n",address, port);
	printf("Sending Request Message for File with File Name : %s...\n",filename);
	char request[50];
	char *buff = "REQUEST : FILE :";
	sprintf(request, "%s %s", buff, filename);

	// Requesting currently connected Peer_Node for desired file or printing an unsuccessful error
	if (write(sock_fd, request, strlen(request)) < 0) {
		printf("ERROR : Cannot Write to Peer_Node\n");
		exit(EXIT_FAILURE);
	}

	// Reading currently connected Peer_Node Response or printing an unsuccessful error
	bzero(buffer, BUFF_LEN);
	if (read(sock_fd, buffer, BUFF_LEN-1) < 0) {
		printf("ERROR : Cannot Read from Peer_Node\n");
		exit(EXIT_FAILURE);
	}
	printf("Peer_Client Received : %s\n", buffer);
	if (strcmp(buffer, "File NOT FOUND") == 0) {
		// Closing Connection
		printf("Gracefully Closing Connection...\nFile NOT FOUND at this Peer_Node\n");
		if (close(sock_fd) < 0) {
			printf("ERROR : Cannot Close Connection with Peer_Node\n");
			exit(EXIT_FAILURE);
		}
	}
	else if (strcmp(buffer, "File FOUND") == 0) {
		printf("Successfully FOUND the file\n");
		if (read(sock_fd, buffer, BUFF_LEN-1) < 0){
			printf("ERROR : Cannot Read from Peer_Node\n");
			exit(EXIT_FAILURE);
		}
		printf("Printing File Content... \n%s\n", buffer);
		printf("Gracefully Closing Connection with Peer_Node....\n");

		// Closing connection or printing an unsuccessful error
		if (close(sock_fd) < 0) {
			printf("ERROR : Cannot Close Connection with Peer_Node");
			exit(EXIT_FAILURE);
		}

		// Create a copy the file for the Peer_Client, too

		FILE *tp = fopen("Peer_Client_Copy.txt", "w");
		fprintf(tp, "%s", buffer);
		fclose(tp);
		printf("Client Copy Created\n");
		return 0;
	}
	else
		printf("ERROR : Unexpected Peer_Node Response. Please Check Syntax\n");
	return -1;
}


void handle_sigint(int sig){
	close(sock_fd);
	fflush(stdout);
	exit(EXIT_FAILURE);
}
