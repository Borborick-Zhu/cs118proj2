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

    // Configure the client address structure from which we receive packets (newly added)
    memset(&client_addr_from, 0, sizeof(client_addr_from));
    client_addr_from.sin_family = AF_INET;
    client_addr_from.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_from.sin_port = htons(CLIENT_PORT);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt
    // size_t bytes_written;
    // size_t bytes_received;

    // // Receiving packet from client
    // bytes_received = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr_from, addr_size);
    // if (bytes_received == -1) {
    //     perror("Error retrieving packet");
    //     close(listen_sockfd);
    //     close(send_sockfd);
    //     return 1;
    // }

    // // If inorder packet arrives
    // if (expected_seq_num == buffer.seqnum) {
    //     // Write the buffer to the output file
    //     bytes_written = fwrite(&(buffer.payload), 1, strlen(buffer.payload), fp);
    //     // Error handling

    //     // Send back an ACK
        
    // } else { // else out of order packet arrived
    //     /* TODO - Implement out of order packet mechanisms */
    // }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
