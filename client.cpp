#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <queue>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    socklen_t ack_addr_size = sizeof(server_addr_from);
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

    // Initialize Congestion Window
    struct packet *cwnd;
    int window_size = 1;
    cwnd = (struct packet *) malloc(window_size * sizeof(struct packet));

    /* Good cwnd debugging code */
    // struct packet test1;
    // struct packet test2; 
    // char buftest[20] = "okay";
    // char buftest2[20] = "not okay";
    // build_packet(&test1, 1, 0, '0', '0', 20, buftest);
    // build_packet(&test2, 2, 0, '0', '0', 20, buftest2);
    // memcpy(&(cwnd[0]), &test1, sizeof(struct packet));
    // memcpy(&(cwnd[1]), &test2, sizeof(struct packet));

    // for (int i = 0; i < window_size; i++) { 
    //      printf("contents: %s\n", cwnd[i].payload);
    // }

   

    // Initializing timeout countdown that times out "recvfrom()" function calls after a specified amt of time
    // tv.tv_sec = TIMEOUT;
    // tv.tv_usec = 0;
    // if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    //     perror("Error setting timeout");
    //     close(listen_sockfd);
    //     close(send_sockfd);
    //     return 1;
    // }

    size_t bytes_read;
    char resend = 0;
    unsigned short total_packets_received = 0;
    int packets_received = 0;
    while(1) {
        // If resending a packet, 
        if (resend) {
            fseek(fp, -bytes_read, SEEK_CUR);
        }

        // Sends one packet per packet received
        while (packets_received != window_size) {
            fseek(fp, (seq_num * PAYLOAD_SIZE), SEEK_SET);
            if ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) < sizeof(buffer)) {
                last = 1;
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
                if (last == 1) {
                    fclose(fp);
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 0;
                }
                seq_num += 1;
            }

            // Waiting for the retrieval of the corresponding ack
            if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &ack_addr_size) == -1) {
                // Timeout happens so set flag to resend
                //if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    //printf("Timeout occurred\n");
                    //resend = 1;
                //} else {
                    perror("Error receiving ACK");
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 1;
                //}
            } else { // ACK has been received
                // 
                if (ack_pkt.acknum == total_packets_received) {
                    total_packets_received += 1;
                    packets_received += 1;
                } else if (ack_pkt.acknum > total_packets_received) {
                    total_packets_received = ack_pkt.acknum;
                    packets_received += ack_pkt.acknum - total_packets_received;
                }// less than so there is a duplicate ack.
                // Otherwise, ACK number does not match the sent/expected sequence number so discard
            }
        }

       // Increasing cwnd size by 1
       increaseWindowSize(&cwnd, &window_size, window_size + 1);

       // Send next two packets
        fseek(fp, (seq_num * PAYLOAD_SIZE), SEEK_SET);
        if ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) < sizeof(buffer)) {
            last = 1;
            memset(buffer + bytes_read, '\0', sizeof(buffer) - bytes_read);
        }

        // Construct first packet from buffer contents
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
            if (last == 1) {
                fclose(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return 0;
            }
            seq_num += 1;
        }

        fseek(fp, (seq_num * PAYLOAD_SIZE), SEEK_SET);
        if ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) < sizeof(buffer)) {
            last = 1;
            memset(buffer + bytes_read, '\0', sizeof(buffer) - bytes_read);
        }

        // Construct first packet from buffer contents
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
            if (last == 1) {
                fclose(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return 0;
            }
            seq_num += 1;
        }

        if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &ack_addr_size) == -1) {
            perror("Error receiving ACK");
            close(listen_sockfd);
            close(send_sockfd);
            return 1;
        } else {
            // 
            if (ack_pkt.acknum == total_packets_received) {
                total_packets_received += 1;
                packets_received = 1;
            } else if (ack_pkt.acknum > total_packets_received) {
                total_packets_received = ack_pkt.acknum;
                packets_received = ack_pkt.acknum - total_packets_received;
            }// less than so there is a duplicate ack.
            // Otherwise, ACK number does not match the sent/expected sequence number so discard
        }
    }
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

