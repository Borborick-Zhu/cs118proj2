#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    int received_packets = 0;

    // Fixed batch of packets
    struct packet batch[WINDOW_SIZE];

    char send = 1;

    while (1) {
        // If supposed to send
        if (send) {
            // Reset some values
            int num_packets_in_window = 0;

            // Construct batch of size WINDOW_SIZE and insert into window
            for (int i = 0; i < WINDOW_SIZE && !feof(fp); i++) {
                // Seek for correct file contents in input file
                fseek(fp, (received_packets * PAYLOAD_SIZE), SEEK_SET);

                // Read a chunk from file
                size_t bytesRead = fread(buffer, 1, PAYLOAD_SIZE, fp);

                // Build a packet and insert it into the i-th position in the buffer
                build_packet(&batch[i], seq_num, ack_num, (bytesRead < PAYLOAD_SIZE), ack, bytesRead, buffer);

                // Increment counter
                num_packets_in_window += 1;

                // Increment the sequence number
                seq_num = (seq_num + 1) % MAX_SEQUENCE;
            }

            // Send entire batch in window
            for (int i = 0; i < num_packets_in_window; i++) {
                sendto(send_sockfd, &batch[i], sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
                printSend(&batch[i], 0);      
            }

            printf("\n");
        }

        // Wait for acknowledgment from the server
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_sockfd, &readfds);

        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;

        int selectResult = select(listen_sockfd + 1, &readfds, NULL, NULL, &tv);

        if (selectResult > 0) {
        
            // Data is available to read
            ssize_t recv_size = recvfrom(listen_sockfd, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)&server_addr_from, &addr_size);

            // The data read is a valid packet
            if (recv_size > 0 && ack_pkt.ack == 1) { 
                // Received an expected ACK
                if (ack_pkt.acknum >= (received_packets % MAX_SEQUENCE)){
                    printRecv(&ack_pkt);
                    
                    // Increment to the next expected seq number
                    received_packets += (ack_pkt.acknum - received_packets) + 1; 

                    // If you've received enough packets to constitute one window length
                    if ((received_packets % WINDOW_SIZE) == 0) {
                        send = 1; // Set flag to send next batch
                    } else {
                        send = 0; // Wait for rest of window
                    }

                    // Set expected ack_num
                    ack_num = ack_pkt.seqnum;

                    // Check if the ack packet is the last one and break if so
                    if (ack_pkt.last) {
                        break;
                    }
                }

            } 
        } else {
            // Handle timeout, retransmit the batch
            for (int i = 0; i < WINDOW_SIZE; i++) {
                printSend(&batch[i], 1);
            }

            // Set send flag
            send = 1;

            // Reset parameters to resend the previous window
            seq_num -= WINDOW_SIZE;
            received_packets -= (received_packets % WINDOW_SIZE);
            printf("\n");
        }
    }


    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
