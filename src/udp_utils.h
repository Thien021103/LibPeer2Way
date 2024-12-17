#ifndef _UDP_UTILS_H
#define _UDP_UTILS_H

#include <arpa/inet.h>


typedef struct _udp_
{
    int sockfd;
    struct sockaddr_in dest_addr;
}udp_t;

int udp_sock_create(udp_t *udp, int dest_port, const char *dest_addr);
int udp_sock_close(udp_t *udp);
int udp_send_frame(udp_t *udp, const uint8_t *data, uint32_t data_len);

#endif //_UDP_UTILS_H