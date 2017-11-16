#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define BACKLOG 10     /* how many pending connections queue will hold */
#define MAXFILENAME 30
#define RCVPATH "./rcv/"
#define MAXBUFFSIZE 1024
#define HEADERSIZE 11

int validateIP4Dotted(const char *s){
	int len = strlen(s);
	if (len < 7 || len > 15)
		return 0;
	char tail[16];
	tail[0] = 0;
	unsigned int d[4];
	int c = sscanf(s, "%3u.%3u.%3u.%3u%s", &d[0], &d[1], &d[2], &d[3], tail);
	if (c != 4 || tail[0])
		return 0;
	for (int i = 0; i < 4; i++)
		if (d[i] > 255)
			return 0;
	return 1;
}


int main(int argc, char *argv[]) {
	FILE* file;
	int sock_fd, comm_fd, PORT, bytes_recv, mssg_len;  		// listen on sock_fd, new connection on comm_fd 
	struct  sockaddr_in my_addr;    // my address information 
	struct  sockaddr_in their_addr; // connector's address information 
	socklen_t addr_size;
	char sig_send[MAXBUFFSIZE+HEADERSIZE], sig_recv[MAXBUFFSIZE+HEADERSIZE], 
		file_name[MAXFILENAME], path[MAXFILENAME+(strlen(RCVPATH))];
	unsigned char unsig_send[MAXBUFFSIZE+HEADERSIZE],
					unsig_recv[MAXBUFFSIZE+HEADERSIZE];

	//Validating user arguments
	if (argc != 3){
		printf("Incorrect use of the program use: <IP> <PORT>. Error\n");
		exit(1);    
	}

	PORT = atoi(argv[2]);

	//Checking if valid IP user input
	int c = strcmp(argv[1], "localhost");
	if (c != 0) {
		if ( (c = validateIP4Dotted(argv[1]) ) == 0) {
			printf("Invalid IP given, try again. Error\n");
			exit(1);    
		}
	}
	printf("Valid IP provided, initializing socket...\n");

	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket\n");
		exit(1);
	}

	my_addr.sin_family = AF_INET;         /* host byte order */
	my_addr.sin_port = htons(PORT);     /* short, network byte order */
	my_addr.sin_addr.s_addr = inet_addr(argv[1]);
	bzero(&(my_addr.sin_zero), 8);        /* zero the rest of the struct */

	if (bind(sock_fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind\n");
		exit(1);
	}

	if (listen(sock_fd, BACKLOG) == -1) {
		perror("listen\n");
		exit(1);
	}

	addr_size = sizeof their_addr;
	if ((comm_fd = accept(sock_fd, (struct sockaddr *)&their_addr, &addr_size)) == -1) {
		perror("accept\n");
	}

	// Get file name from client
	bzero(sig_recv, MAXBUFFSIZE+HEADERSIZE);
	bzero(file_name, MAXFILENAME);
	recv(comm_fd,sig_recv,8,0); // rcv length
	// printf ("****sig_recv(length): %s \n", sig_recv);
	mssg_len = atoi(sig_recv);
	recv(comm_fd,file_name,mssg_len,0); // rcv file_name
	// printf ("****Rcvd: %s - %d\n", file_name, mssg_len);
	strcpy(path, RCVPATH);
	strcat(path, file_name);
	printf("****PATH: %s \n", path);
	file=fopen(path,"wb");
	if (!file){
		printf("Unable to open file!\n");
		return 1;
	}

	printf("Receiving file %s , timer started...\n", file_name); 
	clock_t start = clock(), diff;
	puts("===============FILE TRANSFER START===============\n");
	
	while(1){
		bytes_recv = 0;


		bzero(sig_recv, MAXBUFFSIZE+HEADERSIZE);
		bytes_recv = recv(comm_fd, sig_recv , 8 , 0); //rcv length
		sig_recv[bytes_recv] = '\0';
		mssg_len = atoi(sig_recv);
		// printf("RECV mssg_len: %d , sig_recv: %s\n", mssg_len, sig_recv);

		bzero(unsig_recv, MAXBUFFSIZE+HEADERSIZE);
		bytes_recv = recv(comm_fd, unsig_recv,mssg_len,0); // rcv payload
		unsig_recv[bytes_recv] = '\0';
	  	// printf("RCVD payload: %d bytes- data: %s \n", bytes_recv, unsig_recv);

		printf("Received - %d bytes\n" , bytes_recv);




		if (bytes_recv < 0) { //Error
		    perror("Connection error\n");
		    close(comm_fd);
		    exit(0);
		}
		if (bytes_recv == 0) { //Disconnect
		    perror("Connection closed\n");
		    close(comm_fd);
		    exit(0);
		}
	  	fwrite(unsig_recv, 1, mssg_len, file);

	  	bytes_recv = recv(comm_fd, sig_recv , 3 , 0); //rcv eof
	  	sig_recv[bytes_recv] = '\0';
	  	// printf("RECV eof: %d bytes - data: %s\n", bytes_recv, sig_recv);
	  	if ( (strcmp(sig_recv, "end")) == 0 ){
	  		// printf("Exit loop!\n");
	  		break;
	  	}


	}

	printf("===============FILE TRANSFER END===============\n");



	diff = clock() - start;
	int msec = diff * 1000 / CLOCKS_PER_SEC;
	printf("Time taken to transfer file:  %d secs %d msecs.\n", msec/1000, msec%1000);

	fclose(file);
	close(comm_fd);

	return 0;
}