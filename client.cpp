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
    int state = 0;
    int ssthresh = 1;
    //0 - slow start
    //1 - fast retransmit 
    //2 - congestions avoidance
    printf("start\n");
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short ack_num = 0;
    unsigned short last_seq_num_in_batch;

    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2)
    {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
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
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server

    // - Reads file and sends packets to server, waits for ACK before sending next pkt (stop-and-wait)

    int fin = 0; // when it recieves ack for last packet

    float window_size = 1;
    int last_acked = 0;
    int last_sent = 0;
    int dup_count = 1;
    //window should go from last_acked to last_acked-1 (inclusive)

    //only implement slow start and fast retransmit and go back and forth

    //construct entire packets array by dividing up the original content

    struct packet packets[MAX_SEQUENCE];

    int seq_num = 0;
    int packetCount = 0;


    while (!feof(fp) && packetCount < MAX_SEQUENCE) {
        int read_size = fread(buffer, 1, PAYLOAD_SIZE, fp);
        packets[packetCount].seqnum = seq_num++;
        memcpy(packets[packetCount].payload, buffer, read_size);
        packets[packetCount].length = read_size;
        packets[packetCount].last = (feof(fp) ? 1 : 0);
        packetCount++;
    }
    // fclose(fp);

    while(!fin) //CHANGE
    {

        // std::cout << "a " << number << std::endl;
        // add packet to packets if not already in 

        //send all packets in the window if it hasnt been sent yet
        for(int i = last_acked;i < last_acked + window_size;i++){
            if(i>last_sent){
                pkt = packets[i];
                printf("Sending packet %d\n",i);
                last_sent = i;
                sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
            }
        }
        




        // Set the timeout for receiving ACKs after sending a batch
        struct timeval tv;
        tv.tv_sec = 0; // Timeout in seconds
        tv.tv_usec = 203000; // Additional microseconds
        if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv) < 0)
        {
            perror("Error setting socket timeout\n");
            close(listen_sockfd);
            close(send_sockfd);
            return 1;
        }

        // Wait for ACK for the last packet in the batch
        while (1) {
            if(state ==0){
                if(window_size >= ssthresh){
                    state = 1;
                }
            }
            if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, NULL, NULL) > 0)
            {     
                

                if(ack_pkt.last){
                    printf("recieved last");
                    fin = 1;
                    break;
                }

                printf("recieved ack %d dupe count:%d\n",ack_pkt.seqnum,dup_count);

                //CASE 1: New ACK
                if(ack_pkt.seqnum > last_acked){
                    // dup_count = 1; //reset dupe count
                    last_acked = ack_pkt.seqnum;//set last recieved ack
                    // window_size++;//increase the window *additive* or multiplicatbel increase 
                    if(state ==0){//in slow start
                        window_size++;
                        dup_count =1;

                    }else if (state ==1){//in congestion avoidance
                        window_size = window_size + (1/std::floor(window_size));
                        dup_count =1;
                    }else if (state ==2){//in fast recovery
                        dup_count =1;
                        window_size = ssthresh;
                        state = 1;
                    }
                    break;

                //CASE2: DUPE ACK
                }else if(ack_pkt.seqnum == last_acked){

                    if(state ==0){//in slow start
                        dup_count++;
                    }else if (state ==1){//in congestion avoidance
                        dup_count++;
                    }else if (state ==2){//in fast recovery
                        window_size++;
                    }


                    if( dup_count >=3){
                        if(state ==0 | state ==1 ){//in slow start
                            ssthresh = (window_size +1)/2;
                            window_size = ssthresh +3;
                            state = 2;

                            //resend packet and reset timeout 
                            pkt = packets[last_acked];
                            printf("Resending packet %d due to triple ack\n",last_acked);
                            sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
                            last_sent = last_acked + window_size - 1;
                            //reset timeout
                            struct timeval tv;
                            tv.tv_sec = 0; // Timeout in seconds
                            tv.tv_usec = 203000; // Additional microseconds
                            if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv) < 0)
                            {
                                perror("Error setting socket timeout\n");
                                close(listen_sockfd);
                                close(send_sockfd);
                                return 1;
                            }
                            //wait, give enought time to recieve ack for resent packet
                            // unsigned int microseconds = 203000;
                            // usleep(microseconds);                            
                        }
                        else if (state ==2){//in fast recovery
                            if (window_size < 100){
                                window_size++;
                            }
                            break;
                        }

                    }

                }else{
                    printf("Huh the ack is less than last recieved\n");
                }
                    

            }
            else
            { //timeout

                ssthresh = (window_size / 2 > 1) ? window_size / 2 : 1; // AIMD - Multiplicative Decrease
                window_size =1;
                //resend packet and reset timeout 
                pkt = packets[last_acked];
                printf("Resending packet %d due to timeout\n",last_acked);
                sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);
                last_sent = last_acked + window_size - 1;
                //reset timeout
                struct timeval tv;
                tv.tv_sec = 0; // Timeout in seconds
                tv.tv_usec = 203000; // Additional microseconds
                if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv) < 0)
                {
                    perror("Error setting socket timeout\n");
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 1;
                }
                //wait, give enought time to recieve ack for resent packet
                // unsigned int microseconds = 203000;
                // usleep(microseconds);

                state = 1;

                // break; // Break the loop as timeout occurred
            }
        }

    }

    int timeToClose = 0;
    while(!timeToClose){

        pkt.last = 2;
        sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, addr_size);


        struct timeval tv; // time round trip time plus 100 ms for extra
        tv.tv_sec = 0; // Timeout in seconds
        tv.tv_usec = 203000; // Additional microseconds
        if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv) < 0)
        {
            perror("Error setting socket timeout\n");
            close(listen_sockfd);
            close(send_sockfd);
            return 1;
        }

        while(1){
            if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, NULL, NULL) > 0)
            {       
                if(ack_pkt.last){
                    break; //send affirmation again
                }
            }
            else
            {
                printf("Timeout occurred \n" ); //assume server has closed
                timeToClose = 1;
                break;
            }
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}