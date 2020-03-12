#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h> 

#define BUFF_LEN 1024

int get_file(int sock_fd);
int connect_to_peer(char *address, int port, char *filename);
void handle_sigint(int);

int sock_fd, serv_port, n;
struct sockaddr_in serv_addr;
struct hostent *server;
char buffer[BUFF_LEN];

int main(int argc, char **argv){

	if (argc < 3) {
		fprintf(stderr, "Please Specify IP Address and Port Number\nUsage %s IP_Address PORT\n", argv[0]);
		exit(0);
	}

	serv_port = atoi(argv[2]);

	// Creating a socket
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (sock_fd < 0) {
		perror("ERROR opening socket");
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
		perror("ERROR : Connecting to Relay_Server");
		exit(1);
	}

	// Informing the Relay_Server that the request is by Peer_Client
	printf("Connected to the Relay_Server...\nSending Request message...\n");
	char *req = "REQUEST : Peer_Client";
	n = write(sock_fd, req, strlen(req));

	if (n < 0) {
		perror("ERROR : Writing to Socket");
		exit(1);
	}

	// Reading Relay_Server response (containing addresses of all active Client_Nodes)
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN-1);

	if (n < 0) {
		perror("ERROR : Reading from Socket");
		exit(1);
	}

	printf("%s\n", buffer);

	if (buffer[25] == '1') {
		printf("RESPONSE : Client Accepted\nSuccessfully Connected\nFetching Peer_Node Information...\n");
		n = get_file(sock_fd);

		if (n < 0) {
			perror ("ERROR Getting the Requested File from the Peer_Nodes");
			exit(1);
		}
	}
   else
		printf("Node Not Accepted by the Relay_Server\nTry Again...\n");

	return 0;
}

int get_file(int sock_fd)
{
	// Requesting Active Peer_Node Information
	char *req = "REQUEST : Peer_Node Info", buffer[BUFF_LEN];
	int n;

	// Sending message to Relay_Server
	n = write(sock_fd, req, strlen(req));

	if (n < 0) {
		perror("ERROR : Writing to Socket");
		exit(1);
	}

	// Reading Relay_Server response
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN-1);

	if (n < 0) {
		perror("ERROR : Reading from Socket");
		exit(1);
	}
	printf("Received Response - \n%s\n", buffer);
	printf("Gracefully Closing Connection with Relay_Server....\n");
	n = shutdown(sock_fd, 0);
	if (n < 0) {
		perror("ERROR : Closing Connection");
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
		n = connect_to_peer(peer_name, port, file);
		printf("%s %d %d\n", peer_name, port, n);
		if (n < 0)
			continue;
		else {
			flag = 1;
			break;
		}
	}

	if (!flag)
		printf("File NOT found on ANY Peer_Node\n");

	return 0;
}

int connect_to_peer(char *address, int port, char *filename){
	int sock_fd, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	struct in_addr ipv4addr;
	char buffer[BUFF_LEN];	

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0)
		perror("ERROR : Opening Socket");

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
		perror("ERROR : Connecting");
		exit(1);
	}

	printf ("Connection to the Peer_Node successful\nSending File Request Message with File Name : %s...\n",filename);
	char req[50];
	char *buff = "REQUEST : FILE :";
	sprintf(req, "%s %s", buff, filename);

	// Requesting currently connected Peer_Node for desired file
	n = write(sock_fd, req, strlen(req));

	if (n < 0) {
		perror("ERROR : Writing to Socket");
		exit(1);
	}

	// Reading Server Response
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN-1);
	if (n < 0) {
		perror("ERROR : Reading from Socket");
		exit(1);
	}
	printf("received the reply :-  buffer content: %s\n", buffer);
	if (strcmp(buffer, "File NOT FOUND") == 0) {
		// Closing Connection
		printf("Closing Connection Gracefully File NOT FOUND at this Peer_Node...\n");
		n = shutdown(sock_fd, 0);
		if (n < 0) {
			perror("ERROR : Closing Connection");
			exit(1);
		}
	}
	else if (strcmp(buffer, "File FOUND") == 0) {
		printf("FOUND the file...\n");
		n = read(sock_fd, buffer, BUFF_LEN-1);	//read the file content the peer is sending
		if (n < 0) {
			perror("ERROR reading from socket");
			exit(1);
		}
		printf("File has the following content - \n%s", buffer);
		printf("gracefully closing the connection with the peer....\n");
		n = shutdown(sock_fd, 0);
		if (n < 0) {
			perror("ERROR closing the connection");
			exit(1);
		}		//if error

		//save the file on the client too
		FILE *save = fopen("sample2.txt", "w");
		fprintf(save, "%s", buffer);
		fclose(save);

		return 0;
	}			//if file found
	else
		printf("received unknown reply from the node\n");
	//changes to do : allow for larger file transfer with a larger buffer, or file breakdown.
	//assumption : the portname we save in the file, as peer port+200 and use that
	return -1;
}


void handle_sigint(int sig){
	close(sock_fd);
	fflush(stdout);
	// stdout(flush);
	exit(1);
}
