/**********************************
tcp_ser.c: the source file of the server in tcp transmission 
***********************************/


#include "headsock.h"
#include <stdbool.h>

#define BACKLOG 10
#define ERROR_PROBABILITY 0.1

void str_ser(int sockfd);
int setup_server_socket(struct sockaddr_in *my_addr);
void receive_file(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len);
int verify_file();
bool simulate_error(float error_prob);

int main(void)
{
	struct sockaddr_in my_addr;
	int sockfd = setup_server_socket(&my_addr);

	while (1) {
		str_ser(sockfd);
	}

	close(sockfd);
	return 0;
}

// Simulate an error based on error probability
bool simulate_error(float error_prob) {
    if (error_prob < 0.0 || error_prob > 1.0) {
        fprintf(stderr, "Error: Invalid error probability. Must be between 0 and 1.\n");
        exit(EXIT_FAILURE);
    }

    int error_range = (int)(error_prob * 1000);  // Convert probability to a range between 0 and 999
    int rand_num = rand() % 1000;                // Generate a random number between 0 and 999

    // If the random number falls within the error range, return true (error occurs)
    return rand_num < error_range;
}

// Set up the UDP server socket and return the descriptor
int setup_server_socket(struct sockaddr_in *my_addr)
{
    int sockfd, ret;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0); // Create UDP socket
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    my_addr->sin_family = AF_INET;
    my_addr->sin_port = htons(MYTCP_PORT);
    my_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    bzero(&(my_addr->sin_zero), 8);

    // Bind socket to the address and port
    ret = bind(sockfd, (struct sockaddr *)my_addr, sizeof(struct sockaddr));
    if (ret < 0) {
        perror("Error binding socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server is ready to receive data...\n");
    return sockfd;
}

void str_ser(int sockfd) {
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	
	receive_file(sockfd, &client_addr, addr_len);

	if (verify_file()) {
        printf("✅ Verification successful! Files are identical.\n");
    } else {
        printf("❌ Verification failed. Files are not the same.\n");
    }
}

// Receive file data and save to file
void receive_file(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len)
{
    char buf[BUFSIZE];
    char recvs[DATALEN];
    struct ack_so1 ack;
    FILE *fp;
    int n = 0, end = 0;
    long lseek = 0;

    printf("Receiving data...\n");

    // Receive data until the end of the file
    while (!end) {
        n = recvfrom(sockfd, &recvs, DATALEN, 0, (struct sockaddr *)client_addr, &addr_len);
        if (n == -1) {
            perror("Error receiving data");
            exit(EXIT_FAILURE);
        }

        bool simulateError = simulate_error(ERROR_PROBABILITY);
        if (simulateError) {
            printf("Error Detected!\n");

            ack.check = 0;
            ack.ack_flag = 0;
            if (sendto(sockfd, &ack, 2, 0, (struct sockaddr *)client_addr, addr_len) == -1) {
                printf("Error sending acknowledgment");
                exit(EXIT_FAILURE);
            }

            continue;
        }

        // Check if it's the end of the file
        if (recvs[n - 1] == '\0') {
            end = 1;
            n--; // Ignore the end byte
        }

        memcpy((buf + lseek), recvs, n);
        lseek += n;

        // Send acknowledgment
        ack.ack_flag = 1;
        ack.check = 0;
        if (sendto(sockfd, &ack, 2, 0, (struct sockaddr *)client_addr, addr_len) == -1) {
            printf("Error sending acknowledgment");
            exit(EXIT_FAILURE);
        }
    }

    // Write received data to file
    if ((fp = fopen("myUDPreceive.txt", "wt")) == NULL) {
        printf("Error opening file for writing");
        exit(EXIT_FAILURE);
    }

    fwrite(buf, 1, lseek, fp);
    fclose(fp);

    printf("File successfully received!\nTotal data received: %ld bytes\n", lseek);
}

// Verify if two files are identical
int verify_file()
{
	const char *received_file = "myUDPreceive.txt";
    const char *original_file = "myfile.txt";

    FILE *fp1 = fopen(received_file, "rt");
    FILE *fp2 = fopen(original_file, "rt");

    if (fp1 == NULL || fp2 == NULL) {
        perror("Error opening files for verification");
        if (fp1) fclose(fp1);
        if (fp2) fclose(fp2);
        return 0; // Verification failed
    }

    char ch1, ch2;
    while (((ch1 = fgetc(fp1)) != EOF) && ((ch2 = fgetc(fp2)) != EOF)) {
        if (ch1 != ch2) {
            fclose(fp1);
            fclose(fp2);
            return 0; // Files do not match
        }
    }

    return 1;
}