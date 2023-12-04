#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <vector>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    socklen_t ack_addr_size = sizeof(server_addr_from);
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

    // Configure the server address structure from which we will receive ACK data (newly added)
    memset(&server_addr_from, 0, sizeof(server_addr_from));
    server_addr_from.sin_family = AF_INET;
    server_addr_from.sin_port = htons(SERVER_PORT);
    server_addr_from.sin_addr.s_addr = inet_addr(SERVER_IP);

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

    // Initializing timeout countdown that times out "recvfrom()" function calls after a specified amt of time
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0) {
        perror("Error setting timeout");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    size_t bytes_read;
    char resend = 0;
    while(!last) {
        // If resending a packet, 
        if (resend) {
            fseek(fp, -bytes_read, SEEK_CUR);
        }


        // If bytes read doesn't fill buffer, then assume it's last packet
        if ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) < sizeof(buffer)) {
            last = 1;
            // Clears rest of buffer that wasn't filled
            memset(buffer + bytes_read, '\0', sizeof(buffer) - bytes_read);
        }

        // Construct a packet from buffer contents
        build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, buffer);

        // Fixes bug where contents of buffer in pkt
        if (last == 1) {
            memcpy(pkt.payload, buffer, PAYLOAD_SIZE);
        }

        // Send the packet over the network
        if (sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size) == -1) {
            perror("Error sending packet");
            close(listen_sockfd);
            close(send_sockfd);
            return 1;
        } else {
            printf("Packet sent: ");
            printSend(&pkt, 0);
        }

        // Waiting for the retrieval of the corresponding ack
        if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &ack_addr_size) == -1) {
            // Timeout happens so set flag to resend
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout occurred\n");
                resend = 1;
            } else {
                perror("Error receiving ACK");
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            }
        } else { // ACK has been received
            // ACK number matches the sent/expected sequence number
            if (ack_pkt.acknum == seq_num) {
                printf("ACK %d has been received\n", ack_pkt.acknum);
                resend = 0; // Set resend flag to false
                seq_num += 1; // Increment sequence number
            } 
            // Otherwise, ACK number does not match the sent/expected sequence number so discard
        }
    }
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

