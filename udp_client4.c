/*******************************
tcp_client.c: the source file of the client in tcp transmission 
********************************/

#include "headsock.h"

float str_cli(FILE *fp, int sockfd, struct sockaddr_in *serv_addr, socklen_t addr_len, long *len);                       //transmission function
void tv_sub(struct  timeval *out, struct timeval *in);	    //calcu the time interval between out and in
void print_host_info(struct hostent *sh);
int setup_socket(struct sockaddr_in *ser_addr, struct in_addr **addrs);
FILE *open_file(const char *filename);

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s <hostname>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    struct hostent *sh = gethostbyname(argv[1]);
    if (sh == NULL) {
        perror("Error: Unable to resolve hostname");
        exit(EXIT_FAILURE);
    }

	print_host_info(sh);

	struct sockaddr_in ser_addr;
    struct in_addr **addrs = (struct in_addr **)sh->h_addr_list;

    int sockfd = setup_socket(&ser_addr, addrs);
    FILE *fp = open_file("myfile.txt");

	// Start transmission
	long len;
	float ti = str_cli(fp, sockfd, &ser_addr, sizeof(struct sockaddr), &len);
	float rt = (len/(float)ti);

	printf("Time(ms) : %.3f, Data sent(byte): %d\nData rate: %f (Kbytes/s)\n", ti, (int)len, rt);

	close(sockfd);
	fclose(fp);

	exit(EXIT_SUCCESS);
}

// Open and return the file pointer
FILE *open_file(const char *filename)
{
    FILE *fp = fopen(filename, "r+t");
    if (fp == NULL) {
        perror("Error: File doesn't exist");
        exit(EXIT_FAILURE);
    }
    return fp;
}

// Setup and return the socket descriptor
int setup_socket(struct sockaddr_in *ser_addr, struct in_addr **addrs)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    ser_addr->sin_family = AF_INET;
    ser_addr->sin_port = htons(MYTCP_PORT);
    memcpy(&(ser_addr->sin_addr.s_addr), *addrs, sizeof(struct in_addr));
    bzero(&(ser_addr->sin_zero), 8);

    return sockfd;
}

// Print host information
void print_host_info(struct hostent *sh)
{
    printf("Canonical name: %s\n", sh->h_name);
    for (char **pptr = sh->h_aliases; *pptr != NULL; pptr++) {
        printf("Alias: %s\n", *pptr);
    }

    switch (sh->h_addrtype) {
        case AF_INET:
            printf("Address type: AF_INET\n");
            break;
        default:
            printf("Unknown address type\n");
            break;
    }
}

float str_cli(FILE *fp, int sockfd, struct sockaddr_in *serv_addr, socklen_t addr_len, long *len)
{
	long lsize, ci = 0;
	int n, slen;
	char *buf, sends[DATALEN];

	struct ack_so ack;
	struct timeval sendt, recvt;

	// Get File size and read into buffer
	fseek (fp , 0 , SEEK_END);
	lsize = ftell (fp);
	rewind (fp);
	printf("The file length is %d bytes\n", (int)lsize);
	printf("the packet length is %d bytes\n",DATALEN);

	// allocate memory to contain the whole file.
	buf = (char *)malloc(lsize);
	if (buf == NULL) {
		perror("Error: Memory allocation failed");
		exit(EXIT_FAILURE);
	}

	fread (buf, 1, lsize, fp);
	buf[lsize] = '\0';

	gettimeofday(&sendt, NULL);

	int packets_sent = 0;
	while(ci<= lsize) {
        slen = ((lsize + 1 - ci) <= DATALEN) ? (lsize + 1 - ci) : DATALEN;
        memcpy(sends, (buf + ci), slen);

		packets_sent += 1;
		printf("Sending packet %d... ", packets_sent);

		n = sendto(sockfd, &sends, slen, 0, (struct sockaddr *)serv_addr, addr_len);
		if(n == -1) {
			printf("Error sending data");
			free(buf);
			exit(EXIT_FAILURE);
		}

		// Receive acknowledgment after sending each packet
        n = recvfrom(sockfd, &ack, sizeof(struct ack_so), 0, (struct sockaddr *)serv_addr, &addr_len);
        if (n == -1) {
            printf("Error receiving acknowledgment\ns");
            free(buf);
            exit(EXIT_FAILURE);
        }

        // Validate acknowledgment
        if (ack.num != 1 || ack.len != 0) {
            printf("Error: Invalid ACK received. Transmission failed.\n");
            free(buf);
            exit(EXIT_FAILURE);
        }

		ci += slen;
		printf("ACK received.\n");
	}

	gettimeofday(&recvt, NULL);
	*len= ci;
	tv_sub(&recvt, &sendt);

	// Final log message after successful transmission
    printf("✅ File successfully sent! Transmission completed.\n");

	float time_inv = (recvt.tv_sec)*1000.0 + (recvt.tv_usec)/1000.0;
	free(buf);

	return(time_inv);
}

void tv_sub(struct  timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0)
	{
		--out ->tv_sec;
		out ->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}
