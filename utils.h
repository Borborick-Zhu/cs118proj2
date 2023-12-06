#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MACROS
#define SERVER_IP "127.0.0.1"
#define LOCAL_HOST "127.0.0.1"
#define SERVER_PORT_TO 5002
#define CLIENT_PORT 6001
#define SERVER_PORT 6002
#define CLIENT_PORT_TO 5001
#define PAYLOAD_SIZE 1024
#define WINDOW_SIZE 5
#define TIMEOUT 1
#define MAX_SEQUENCE 1024



// Packet Layout
// You may change this if you want to
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char ack;
    char last;
    unsigned int length;
    char packet_check[7];
    int window_size;
    char payload[PAYLOAD_SIZE];
    
};

// Utility function to build a packet
void build_packet(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char last, char ack, unsigned int length, const char* payload, int window_size) {
    strcpy(pkt->packet_check, "packet");
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->ack = ack;
    pkt->last = last;
    pkt->length = length;
    pkt->window_size = window_size;
    memcpy(pkt->payload, payload, length);
}

// Utility function to print a packet
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", (pkt->ack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
}

//utility function to increase window size: 
void increaseWindowSize(struct packet** cwnd, int* window_size, int new_size) {
    // Create a new buffer with the increased size
    struct packet* new_cwnd = (struct packet*)malloc(new_size * sizeof(struct packet));

    // struct packet dummy = (struct packet*)malloc(sizeof(struct packet));

    // build_packet(&dummy, -1, -1, 1, 1, -1, "", -1);

    if (new_cwnd == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Initialize all of newcwnd to be dummy packet
    // for (int i = 0; i < new_size; i++){
    //     memset(&(new_cwnd[i]), -1, sizeof(struct packet));
    // }

    // Copy the existing contents to the new buffer
    for (int i = 0; i < *window_size; ++i) {
        new_cwnd[i] = (*cwnd)[i];
    }

    // Free the old buffer
    free(*cwnd);

    // Update the window size and buffer pointer
    *window_size = new_size;
    *cwnd = new_cwnd;
}

int isMemoryAllOnes(const struct packet* ptr, size_t size) {
    const unsigned char* bytes = (const unsigned char*)ptr;

    for (size_t i = 0; i < size; ++i) {
        if (bytes[i] != 0xFF) {  // Check if all bits are set to 1
            return 0;  // Not all 1's
        }
    }

    return 1;  // All 1's
}

#endif