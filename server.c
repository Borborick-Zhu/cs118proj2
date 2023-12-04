#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt
    while (1) {
        // Receive a packet from the client
        recv_len = recvfrom(listen_sockfd, &buffer, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_from, &addr_size);
        if (recv_len > 0) {
            printRecv(&buffer);

            // Check if the received packet has the expected sequence number
            if (buffer.seqnum == expected_seq_num) {
                // Write the payload to the file
                fwrite(buffer.payload, 1, buffer.length, fp);

                // Build and send acknowledgment to the client
                build_packet(&ack_pkt, 0, expected_seq_num, 0, 1, 0, NULL);
                sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
                printSend(&ack_pkt, 0);

                // Move to the next expected sequence number
                expected_seq_num = (expected_seq_num + 1) % MAX_SEQUENCE;

                if (buffer.last) {
                    // Last packet received, break out of the loop
                    break;
                }
            } else {
                // Out-of-order packet received, discard and request retransmission of the expected packet
                build_packet(&ack_pkt, 0, (expected_seq_num - 1 + MAX_SEQUENCE) % MAX_SEQUENCE, 0, 1, 0, NULL);
                sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
                printSend(&ack_pkt, 0);
            }
        } else {
            // Handle packet loss or error, request retransmission of the expected packet
            build_packet(&ack_pkt, 0, (expected_seq_num - 1 + MAX_SEQUENCE) % MAX_SEQUENCE, 0, 1, 0, NULL);
            sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
            printSend(&ack_pkt, 0);
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}