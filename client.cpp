#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "utils.h"
#include <cmath>




int main(int argc, char *argv[])
{
    //INITIALIZE VARIABLES:
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];

    //INITIALIZE FILE TRANSFER
    if (argc != 2)
    {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0)
    {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0)
    {
        perror("Could not create send socket");
        return 1;
    }

    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    float cwnd = 1;
    int desired_ack = 0; // next packet thats we want to be acked (its seq num)
    int latest_sent = -1; // latest sent packet (seq num)
    int dup = 0; // number of duplicates. 
    int ssthresh = 5; // slow start threshold

    //window should go from last_acked to last_acked-1 (inclusive)

    //only implement slow start and fast retransmit and go back and forth

    //construct entire packets array by dividing up the original content

    struct packet packet_buffer[4196];
    int total_packets = 0;

    // Fast retransmit flag
    char fr = 0;

    //read all the files in to the packet_buffer
    while (!feof(fp) && total_packets < 4196) {
        ssize_t bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
        build_packet(&packet_buffer[total_packets], total_packets, 0, (bytes_read < PAYLOAD_SIZE), 0, bytes_read, buffer);
        total_packets += 1;
    }

    // Timeout values
    struct timeval tv; 
    tv.tv_sec = 0; 
    tv.tv_usec = 210000; 

    while (1) { // break out of the while loop when the last ack comes back.

        //send all packets in the window if not already sent. 
        for (int i = desired_ack; i < desired_ack + cwnd; i++) {
            if (i > latest_sent) {
                latest_sent = i;
                sendto(send_sockfd, &packet_buffer[i], sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
                if (packet_buffer[i].last) {
                    break;
                }
                printf("Sending packet %d\n", i);
                printf("Desired ACK: %d\n", desired_ack);
            }
        }

        // Set timeout after sending
        if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0) {
            perror("Setting timeout error\n");
            fclose(fp);
            close(listen_sockfd);
            close(send_sockfd);
            return -1;
        }

        //logic to wait for the acks now. 
        while (1) {
            //if we receive acks. 
            if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size) > 0) {

                if (ack_pkt.last) { // if receive last ack, break out of the loop
                    fclose(fp);
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 0;
                }

                //ack cases: 
                // if regular ack is received or if it is greater than. 
                if (ack_pkt.acknum >= desired_ack) {
                    printRecv(&ack_pkt);
                    
                    // Congestional Avoidance
                    if (cwnd > ssthresh) {
                        cwnd += (1.0 / (int) cwnd);
                    } else { // Slow Start
                        cwnd += 1;
                    }
                    printf("cwnd is now: %f\n", cwnd);

                    //change the value of the next desired acked. 
                    desired_ack = ack_pkt.acknum + 1;

                    // Reset dup pkt counter
                    dup = 0;

                    // Check if this was a fast retransmit case
                    if (fr) {
                        // Reduce window size after receiving retransmission to be ssthresh (removing all duplicates)
                        cwnd  = ssthresh;

                        // Reset the fast retransmit flag
                        fr = 0;
                    }

                    break;
                } else if (ack_pkt.acknum == desired_ack - 1) { // if duplicate ack is received. 
                    // Fast retransmit case
                    printf("Received a dupe packet: ");
                    printRecv(&ack_pkt);

                    // Increment dup packet counter
                    dup += 1;

                    // If dup pkts received is 3
                    if (dup == 3) {
                        // Change ssthresh to be max(2, (cwnd / 2))
                        ssthresh = 2 < std::floor(cwnd / 2) ? std::floor(cwnd / 2) : 2;

                        // Alter window size
                        cwnd = ssthresh + 3;

                        // Set fast retransmit flag
                        fr = 1;

                        // Retransmit the requested packet
                        sendto(send_sockfd, &packet_buffer[desired_ack - 1], sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
                        printSend(&packet_buffer[desired_ack - 1], 1);

                        // Set timeout after sending
                        if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0) {
                            perror("Setting timeout error\n");
                            fclose(fp);
                            close(listen_sockfd);
                            close(send_sockfd);
                            return -1;
                        }
                    } else if (dup > 3) { // If the counter exceeds 3
                        // Increment window by 1
                        cwnd += 1;
                    }   
                } // Otherwise we get an ack less than what we're expecting
            } else { // we timeout. 
                // Change ssthresh to be max(2, (cwnd / 2))
                ssthresh = 2 < std::floor(cwnd / 2) ? std::floor(cwnd / 2) : 2;

                // Reset window size
                cwnd = 1;
                
                printf("Timeout: ");
                printSend(&packet_buffer[desired_ack - 1], 1);
                // Retransmit the beginning of the window
                sendto(send_sockfd, &packet_buffer[desired_ack - 1], sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));

                // Reset the timeout
                if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                    perror("Setting timeout error\n");
                    fclose(fp);
                    close(listen_sockfd);
                    close(send_sockfd);
                    return -1;
                }
                
                break;
            }
        }
        printf("\n");
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}