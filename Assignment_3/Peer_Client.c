#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFF_LEN 1024

int get_file(int sock_fd);
int connect_to_peer(char *address, int port, char *filename);

int main(int argc, char **argv){
	int sock_fd, serv_port, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	char buffer[BUFF_LEN];

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

	/* Now connect to the server */
	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR Connecting");
		exit(1);
	}

	/* Now ask for a message from the user, this message
	 * will be read by server
	 */

	printf("Connecting to the relay server.Sending Request message\n");
	char *req = "REQUEST : Peer_Client";

	/* Send message to the server */
	n = write(sock_fd, req, strlen(req));

	if (n < 0) {
		perror("ERROR Writing to Socket");
		exit(1);
	}

	/* Now read server response */
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN-1);

	if (n < 0) {
		perror("ERROR Reading from Socket");
		exit(1);
	}

	printf("%s\n", buffer);

	//start server if node accepted by relay
	if (buffer[25] == '1') {
		printf("RESPONSE : Client Accepted\nSUCESSFULLY Connected\nFetcing Peer_Node Information\n");
		n = get_file(sock_fd);

		if (n < 0) {
			perror
			    ("ERROR Getting the Requested File from the Peer_Nodes");
			exit(1);
		}
	}
   else
		printf("Node Not Accepted by the Relay_Server\nTry Again...\n");

	return 0;
}

int get_file(int sock_fd)
{
	// Requesting Active Peer Information
	char *req = "REQUEST : Peer_Node Info", buffer[BUFF_LEN];
	int n;

	// Sending message to Relay_Server
	n = write(sock_fd, req, strlen(req));

	if (n < 0) {
		perror("ERROR writing to socket");
		exit(1);
	}

	// Reading Relay_Server response
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN-1);

	if (n < 0) {
		perror("ERROR reading from socket");
		exit(1);
	}
	printf("Receive the following response - \n%s\n", buffer);
	printf("gracefully closing the connection with the relay server....\n");
	n = shutdown(sock_fd, 0);
	if (n < 0) {
		perror("ERROR closing the connection");
		exit(1);
	}

	// Storing Peer_Nodes information in a file
	FILE *peers = fopen("Peer_Nodes_Info_at_Peer_Client.txt", "w");
	fprintf(peers, "%s", buffer);
	fclose(peers);

	char file[50];
	printf("Enter the File Name : ");
	scanf("%s", file);

	//process the response one peer at a time and try to fetch the file
	char peer_name[INET_ADDRSTRLEN];
	int port, flag = 0;
	peers = fopen("Peer_Nodes_Info_at_Peer_Client.txt", "r");
	while (fscanf(peers, "%s %d", peer_name, &port) != EOF) {
		printf("Connecting to the Peer_Node %s : %d...\n", peer_name, port);
		n = connect_to_peer(peer_name, port, file);
		if (n < 0)
			continue;
		else {
			flag = 1;
			break;
		}		//successfult found the file on this node
	}
	fclose(peers);
	if (!flag)
		printf("File not found on any node!\n");

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
		perror("ERROR opening socket");

	inet_pton(AF_INET, address, &ipv4addr);
	server = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);

	if (server == NULL) {
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
	      server->h_length);
	serv_addr.sin_port = htons(port);

	/* Now connect to the server */
	if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR connecting");
		exit(1);
	}

	/* Now ask for a message from the user, this message
	 * will be read by server
	 */

	printf ("Connection to the Peer SUCCESSFUL.\nSending File transfer Request message with the file name....\n");
	char req[50];
	char *buff = "REQUEST : FILE :";
	sprintf(req, "%s %s", buff, filename);

	/* Send message to the server */
	n = write(sock_fd, req, strlen(req));
	if (n < 0) {
		perror("ERROR writing to socket");
		exit(1);
	}

	/* Now read server response */
	bzero(buffer, BUFF_LEN);
	n = read(sock_fd, buffer, BUFF_LEN-1);
	if (n < 0) {
		perror("ERROR reading from socket");
		exit(1);
	}
	printf("received the reply :-%s\n", buffer);
	if (strcmp(buffer, "File NOT FOUND") == 0) {
		//close the connection gracefully since file not found
		printf("Closing the connection gracefully since file NOT FOUND on this node...\n");
		n = shutdown(sock_fd, 0);
		if (n < 0) {
			perror("ERROR closing the connection");
			exit(1);
		}
	}			//if file is not found
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
		FILE *save = fopen("sample1.txt", "w");
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
