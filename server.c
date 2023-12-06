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
    //int recv_len;
    struct packet ack_pkt;
    const char* payload = "";

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
    size_t bytes_received;

    // Creates a buffer of size 1
    struct packet cache[4096];

    while(1) {

        // Receiving packet from client
        bytes_received = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr_from, &addr_size);
        if (bytes_received == (unsigned long)-1) {
            perror("Error retrieving packet");
            close(listen_sockfd);
            close(send_sockfd);
            return 1;
        }

        // If the received packet is in-order
        if (expected_seq_num == buffer.seqnum) {
            printf("Just received packet with sequence number: %d\n", expected_seq_num);

            cache[expected_seq_num] = buffer; //write it into the buffer.

            // Check front of cache and write, incrementing expected seq_num
            while (1) {
                if (strcmp(cache[expected_seq_num].packet_check, "packet") == 0) {
                    
                    // Write front of cache 
                    //printf("payload: %s\n", cache[0].payload);
                    if (cache[expected_seq_num].last == 1) {
                        fwrite(cache[expected_seq_num].payload, 1, strlen(cache[expected_seq_num].payload), fp);
                        fclose(fp);
                        close(listen_sockfd);
                        close(send_sockfd);
                        return 0;
                    } 
                    fwrite(cache[expected_seq_num].payload, 1, PAYLOAD_SIZE, fp);
                    
                    
                    //printf("Written packet with seqnum %d\n", cache[0].seqnum);
                    // Increment expected sequence number
                    expected_seq_num += 1;
                } else {
                    //printf("not a packet, string is: %s\n", cache[0].packet_check);
                    break;
                }
            }

            // Construct an ACK packet based on if the packet received is LAST or not
            build_packet(&ack_pkt, 0, expected_seq_num - 1, buffer.last, 1, 0, payload);

            // Send back an ACK
            if (sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size) == -1){
                perror("Error sending ACK");
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            } else {
                printf("Ack packet %d was sent back to client\n", ack_pkt.acknum);
            }
        } else { // else out of order packet arrived (buffer the out of order packet)
            
            // Place out-of-order packet into buffer at correct position
            cache[buffer.seqnum] = buffer;

            printf("Received pkt seqnum %d but expected %d\n", buffer.seqnum, expected_seq_num);
            //printf("Current Client window size: %d\n", buffer.window_size);

            // Construct a "retransmission" ACK packet using the previous seq number
            build_packet(&ack_pkt, 0, expected_seq_num, 0, 1, 0, payload);

            // Retransmit the ACK
            if (sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size) == -1){
                perror("Error sending ACK");
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            } else {
                printf("Ack packet %d was retransmitted back to client\n", ack_pkt.acknum);
            }
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
