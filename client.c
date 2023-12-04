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
    while (1) {
        fseek(fp, (received_packets * PAYLOAD_SIZE), SEEK_SET);

        // Read a chunk from the file
        size_t bytesRead = fread(buffer, 1, PAYLOAD_SIZE, fp);

        // Build a packet
        build_packet(&pkt, seq_num, ack_num, (bytesRead < PAYLOAD_SIZE), ack, bytesRead, buffer);

        // Send the packet to the server
        sendto(send_sockfd, &pkt, sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
        printSend(&pkt, 0);

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
            if (recv_size > 0 && ack_pkt.ack == 1 && ack_pkt.acknum == seq_num) {
                printRecv(&ack_pkt);

                // Move to the next sequence number
                seq_num = (seq_num + 1) % MAX_SEQUENCE;
                ack_num = ack_pkt.seqnum;
                received_packets += 1;

                if (bytesRead < PAYLOAD_SIZE && ack_pkt.last) {
                    // End of file and last acknowledgment received, break out of the loop
                    break;
                }
            } else {
                // Handle timeout or packet loss, retransmit the packet
                printSend(&pkt, 1);
            }
        } else {
            // Handle timeout, retransmit the packet
            printSend(&pkt, 1);
        }
    }


    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
