#ifndef UDP_COMMUNICATION_H
#define UDP_COMMUNICATION_H

#include <stdint.h>
#include <sys/socket.h>
#include <lwip/netdb.h>

#include "data_structures.h"

// Function prototypes
int udp_init_socket(void);

int udp_send_packet_binary(const message_t *msg, const struct sockaddr_storage *dest);
int udp_send_packet_proto(const message_t *msg, const struct sockaddr_storage *dest);
int udp_send_packet_json(const message_t *msg, const struct sockaddr_storage *dest);
int udp_send_packet_tlv(const message_t *msg, const struct sockaddr_storage *dest);

int udp_receive_packet_binary(message_t *out, struct sockaddr_storage *src);
int udp_receive_packet_proto(message_t *out, struct sockaddr_storage *src);
int udp_receive_packet_json(message_t *out, struct sockaddr_storage *src);
int udp_receive_packet_tlv(message_t *out, struct sockaddr_storage *src);

void udp_close_socket(void);

#endif // UDP_COMMUNICATION_H
