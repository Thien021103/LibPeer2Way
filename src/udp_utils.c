#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include "udp_utils.h"
#include <sys/time.h>
#include <pthread.h>


#define UDP_MTU 1400

int udp_sock_create(udp_t *udp, int dest_port, const char *dest_addr)
{
    udp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp->sockfd < 0)
    {
        perror("Error: Create socket failed\n");
        exit(EXIT_FAILURE);
    }
    // Set SO_REUSEADDR option
    int reuse = 1;
    if (setsockopt(udp->sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(udp->sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&udp->dest_addr, 0, sizeof(struct sockaddr_in));
    udp->dest_addr.sin_family = AF_INET;
    udp->dest_addr.sin_port   = htons(dest_port);
    udp->dest_addr.sin_addr.s_addr = INADDR_ANY;
    // udp->dest_addr.sin_addr.s_addr = inet_addr(dest_addr);
    return 0;
}

int udp_sock_close(udp_t *udp)
{
    if(!udp) return -1;
    close(udp->sockfd);
    return 0;
}

int udp_send_frame(udp_t *udp, const uint8_t *data, uint32_t data_len)
{
    char buffer[UDP_MTU]; // Buffer for each chunk
    int total_chunks = (data_len + UDP_MTU - 1) / UDP_MTU; // Calculate total chunks

    for (int i = 0; i < total_chunks; i++) 
    {
        int offset = i * UDP_MTU;
        int chunk_size = (i == total_chunks - 1) ? (data_len - offset) : UDP_MTU;
        
        // Copy data for the current chunk
        memcpy(buffer, data + offset, chunk_size);
        if(sendto(udp->sockfd, buffer, chunk_size, 0, (const struct sockaddr*)&udp->dest_addr, sizeof(udp->dest_addr)) < 0)
        {
            printf("Error: send udp (sockfd = %d) failed.\n", udp->sockfd);
            return -1;
        }
    }
    return 0;
}