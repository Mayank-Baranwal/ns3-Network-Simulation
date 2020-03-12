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
		fprintf(stderr, "Provide IP Address and Port Number\nFormat %s IP_Address PORT\n", argv[0]);
		exit(0);
	}

	serv_port = atoi(argv[2]);

	// Creating a socket
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (sock_fd < 0) {
		printf("ERROR opening socket");
		exit(1);
	}

	server = gethostbyname(argv[1]);

	if (server == NULL) {
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
	      server->h_length);
	serv_addr.sin_port = htons(serv_port);

	// Connecting to Relay_Server
	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR : Unable to Connect to Relay_Server");
		exit(1);
	}

	// Informing the Relay_Server that the request is by Peer_Client
	printf("Relay_Server Connection Established...\nSending REQUEST message...\n");
	char request[100] = "REQUEST : Peer_Client";
	if (write(sock_fd, request, strlen(request)) < 0) {
		printf("ERROR : Unable to Write to Socket");
		exit(1);
	}

	// Reading Relay_Server response (containing addresses of all active Client_Nodes)
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN-1);

	if (n < 0) {
		printf("ERROR : Reading from Socket");
		exit(1);
	}

	printf("%s\n", buffer);

	if (buffer[25] == '1') {
		printf("RESPONSE : Client Accepted\nSUCCESSFUL Connection\nFetching Peer_Node Information...\n");
		n = find_peer_list(sock_fd);

		if (n < 0) {
			printf("ERROR : Unable to Retrieve Peer_Nodes");
			exit(1);
		}
	}
   else
		printf("Node Rejected by the Relay_Server\nExiting...\n");

	return 0;
}

int find_peer_list(int sock_fd)
{
	// Requesting Active Peer_Node Information
	char *request = "REQUEST : Peer_Node Info", buffer[BUFF_LEN];
	int n;

	// Sending message to Relay_Server
	n = write(sock_fd, request, strlen(request));

	if (n < 0) {
		printf("ERROR : Writing to Socket");
		exit(1);
	}

	// Reading Relay_Server response
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN-1);

	if (n < 0) {
		printf("ERROR : Unable to Read from Socket");
		exit(1);
	}
	printf("Received Response - \n%s\n", buffer);
	printf("Gracefully Closing Connection with Relay_Server....\n");
	n = shutdown(sock_fd, 0);
	if (n < 0) {
		printf("ERROR : Closing Connection");
		exit(1);
	}


	char file[50];
	printf("Enter the File Name : ");
	scanf("%s", file);

	// Trying to retrieve file from Peer_Nodes one at a time
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
		n = peer_connect(peer_name, port, file);
		printf("%s %d %d\n", peer_name, port, n);
		if (n < 0)
			continue;
		else {
			flag = 1;
			break;
		}
	}

	if (!flag)
		printf("File DOES NOT exist on ANY Peer_Node\n");

	return 0;
}

int peer_connect(char *address, int port, char *filename){
	int sock_fd, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	struct in_addr ipv4addr;
	char buffer[BUFF_LEN];	

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0)
		printf("ERROR : Unable to Open Socket");

	inet_pton(AF_INET, address, &ipv4addr);
	server = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);

	if (server == NULL) {
		fprintf(stderr, "ERROR : No such Host\n");
		exit(0);
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(port);

	// Connecting to Peer_Node specified in input fields
	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR : Connecting");
		exit(1);
	}

	printf ("SUCCESSFUL Connection to the Peer_Node\n");
	printf("Sending Request for Message File with File Name : %s...\n",filename);
	char request[50];
	char *buff = "REQUEST : FILE :";
	sprintf(request, "%s %s", buff, filename);

	// Requesting currently connected Peer_Node for desired file
	if (write(sock_fd, request, strlen(request)) < 0) {
		printf("ERROR : Unable to Write to Socket");
		exit(1);
	}

	// Reading Server Response
	bzero(buffer, BUFF_LEN);
	if (read(sock_fd, buffer, BUFF_LEN-1) < 0) {
		printf("ERROR : Unable to Read from Socket");
		exit(1);
	}
	printf("received the reply :-  buffer content: %s\n", buffer);
	if (strcmp(buffer, "File NOT FOUND") == 0) {
		// Closing Connection
		printf("Gracefully Closing Connection...File DOES NOT Exist at this Peer_Node\n");
		if (shutdown(sock_fd, 0) < 0) {
			printf("ERROR : Closing Connection");
			exit(1);
		}
	}
	else if (strcmp(buffer, "File FOUND") == 0) {
		printf("FOUND the file...\n");
		if (read(sock_fd, buffer, BUFF_LEN-1) < 0){ 	//read the file content the peer is sending
			printf("ERROR reading from socket");
			exit(1);
		}
		printf("File description - \n%s", buffer);
		printf("Gracefully Closing Connection with Peer_Node....\n");
		if (shutdown(sock_fd, 0) < 0); {
			printf("ERROR : Aborting the connection");
			exit(1);
		}		//if error

		//tp the file on the client too
		FILE *tp = fopen("Peer_Copy.txt", "w");
		fprintf(tp, "%s", buffer);
		fclose(tp);

		return 0;
	}
	else
		printf("Peer_Node Unresponsive\n");
	return -1;
}


void handle_sigint(int sig){
	close(sock_fd);
	fflush(stdout);
	// stdout(flush);
	exit(1);
}
	