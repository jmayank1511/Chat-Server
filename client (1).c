#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#define PORT 8887
int sockfd;
void error(char *msg)
{
	perror(msg);
	exit(0);
}
void handle_sigint(int sig)
{
	int n = write(sockfd,"/quit ",6);
	if (n <= 0)
		error("ERROR writing to socket");	
	exit(0);
}
int main(int argc, char *argv[])
{
	signal(SIGINT, handle_sigint);
	int portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char buffer[1024];
	int opt =1;
	portno = PORT;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	server = gethostbyname("localhost");
	if (server == NULL)
	{
		fprintf(stderr,"ERROR, no such host");
		exit(0);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(portno);

	if (connect(sockfd,&serv_addr,sizeof(serv_addr)) < 0)
		error("ERROR connecting");

	bzero(buffer,strlen(buffer));
	n = read(sockfd,buffer,1024);   //To read the ack msg of connection
	if (n <= 0)
		error("ERROR reading from socket");

	printf("%s\n",buffer); //Ack Msg
	if(buffer[0]=='M') // if max client limit exceeded then exit
		exit(0);
	//Client is Now Connected to the Server
	printf("Please enter the command or Press Q to Quit\n ");
	int childPid = fork(); 
	if(childPid==0) // process to send the data to sender
	{
		while(1)  // loops until user press Q
		{

			char bufferWrite[1024];
			bzero(bufferWrite,1024);
			fgets(bufferWrite,1023,stdin);
			fflush(stdout);
			if(bufferWrite[0]=='Q') // Pressing Q disconnects the client from the server
			{
				close(sockfd); 
				printf("Disconnected..\n");
				exit(0);				
			}
			n = write(sockfd,bufferWrite,strlen(bufferWrite));
			if (n <= 0)
				error("ERROR writing to socket");
			
		}
	}
	else  // process to receive data from the server
	{
		while(1)
		{

			char bufferRead[1024];
			bzero(bufferRead,1024);
			n = read(sockfd,bufferRead,1023);
			if (n <= 0)
			{
				printf("Disconnected from Server !\n");
				exit(0); // todo: signal all clients
			}
			else 
			{
				printf("%s\n",bufferRead);
			}
		}
	}
	return 0;
}