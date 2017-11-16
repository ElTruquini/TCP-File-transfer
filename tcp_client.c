
#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netdb.h> 
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <unistd.h>
#include <time.h>
#include <assert.h>

#define MAXFILENAME 30
#define MAXBUFFSIZE 1024
#define HEADERSIZE 11 //8 for len + 3 for EOF


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

static char *strpadleft(const char * string, char pad, size_t bytes){
    size_t ssize = strlen(string);
    size_t bits = bytes * 8;
    char *pad_string = (char *) malloc(bits + 1);
    assert(ssize < bits);
    memset(pad_string, pad, bits - ssize);
    strcpy(pad_string + bits - ssize, string);
    return pad_string;
}

static void pad_string(char * stringy, char* sig_send){
	char* hpad;
	char len_line[HEADERSIZE];
	bzero(len_line, HEADERSIZE);
	// printf("stringy: %s - sig_send:%s \n", stringy, sig_send);
	size_t s = strlen(stringy);
	snprintf(len_line, sizeof(len_line), "%zu", s);
	// printf ("len_line is: %s\n", len_line);
	strcat(sig_send, len_line);
	// printf ("After adding length to sig_send: %s\n", sig_send);
	hpad = strpadleft(sig_send, '0', 1);
	// printf("hpad: %s \n ", hpad);
	strcpy(sig_send, hpad);
	strcat(sig_send, stringy);
	// printf ("final padding: %s\n", sig_send);	
}


static void pad_num(int i, char* sig_send){
	char num [HEADERSIZE];
	sprintf(num, "%d", i);
	// printf("i: %d - sig_send:%s \n", i, sig_send);
	strcat(sig_send, num);
	// printf ("After adding i to sig_send: %s\n", sig_send);
	

	char* hpad = strpadleft(sig_send, '0', 1);
	// strpadleft(sig_send, '0', 1);
	strcpy(sig_send, hpad);

	// printf("pad_num - sig_send: %s \n ", sig_send);
}



int main(int argc, char *argv[]){

	FILE* file;
	int sock_fd, PORT, bytes_sent;  
	char file_name[MAXFILENAME],
		 sig_send[MAXBUFFSIZE+HEADERSIZE], 
		 sig_recv[MAXBUFFSIZE+HEADERSIZE];
		 // len_line[HEADERSIZE];
	unsigned char unsig_send[MAXBUFFSIZE+HEADERSIZE],
					unsig_recv[MAXBUFFSIZE+HEADERSIZE];

	// char* hpad; 
	struct sockaddr_in their_addr; 

	//Validating user arguments
	if (argc != 4){
		printf("Incorrect use of the program use: <IP> <PORT> <FILE>. Error\n");
		exit(1);	
	}
	//Check for valid file name length
	if ( (int)strlen(argv[3]) >= MAXFILENAME){
		printf ("File name too long. Error\n");
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
	printf("Valid IP provided, connecting to server...\n");


	// Connection parameters
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket setup. Error\n");
		exit(1);
	}
	bzero(&(their_addr.sin_zero), 8);     /* zero the rest of the struct */
	their_addr.sin_family = AF_INET;      /* host byte order */
	their_addr.sin_port = htons(PORT);    /* short, network byte order */
	their_addr.sin_addr.s_addr = inet_addr(argv[1]);
	if (connect(sock_fd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect\n");
		exit(1);
	}

	// Sending file name to server
	strncpy(file_name, argv[3], MAXFILENAME);
	file=fopen(file_name,"rb");
	if (!file){
		printf("Unable to open file!\n");
		return 1;
	}
	bzero(sig_send, MAXBUFFSIZE+HEADERSIZE);
	pad_string(file_name, sig_send);
	send(sock_fd, sig_send, strlen(sig_send),0);
	// printf ("Sending filename: %s | len_line: %zu \n", sig_send, strlen(sig_send) );


	// File transfer from client to server
	printf("Sending file %s, timer started...\n", file_name);
	clock_t start = clock(), diff;
	puts("===============FILE TRANSFER START===============\n");
	while (1){
		bzero(sig_recv, MAXBUFFSIZE+HEADERSIZE);
		bzero(sig_send, MAXBUFFSIZE+HEADERSIZE);
		bzero(unsig_send, MAXBUFFSIZE+HEADERSIZE);
		bytes_sent = 0;

		// Reading bytes from file
		int i = fread(unsig_send, 1, MAXBUFFSIZE-1, file); 
		unsig_send[i] = '\0';
		// printf("fread - len: %d - data:%s \n", i, unsig_send);
		pad_num(i, sig_send); //bytes read from file
		
		// Seinding length
		bytes_sent = send(sock_fd, sig_send, 8, 0); 
		// printf("Sent len: %s - size: %d \n" , sig_send, 8);
		
		// Seinding payload
		bytes_sent = send(sock_fd, unsig_send, i, 0); 
		// printf("SENDING payload - %d bytes - size: %d- payload: %s \n" , bytes_sent,i, unsig_send);
		printf("Sent - %d bytes\n" , bytes_sent);

		// EOF check
		if (feof(file)){
			bzero(sig_send, MAXBUFFSIZE+HEADERSIZE);
			strcpy(sig_send, "end");
			bytes_sent = send(sock_fd, sig_send, strlen(sig_send), 0);
			// printf("EOF_flag - bytes_sent: %d  - %s - size: %zu\n ", bytes_sent, sig_send, strlen(sig_send));
			break;

		}else{
			bzero(sig_send, MAXBUFFSIZE+HEADERSIZE);
			strcpy(sig_send, "con");
			bytes_sent = send(sock_fd, sig_send, strlen(sig_send), 0);
			// printf("EOF_flag - bytes_sent: %d  - %s\n ", bytes_sent, sig_send);
		}
	}
	puts("===============FILE TRANSFER END===============\n");

	diff = clock() - start;
	int msec = diff * 1000 / CLOCKS_PER_SEC;
	printf("Time taken to rcv file:  %d secs %d msecs. \n", msec/1000, msec%1000);


	fclose(file);


	close(sock_fd);
	return 0;
}