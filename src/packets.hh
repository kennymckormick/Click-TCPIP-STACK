#ifndef PACKETS_HH
#define PACKETS_HH

#define DATA 1
#define ACK 2
#define SYN 4
#define FIN 8
#define HELLO 16
#define RESP 32
#define EXCHANGE 64
#define FAILURE 128
struct MyAPPHeader{
    uint32_t dst_ip;
    uint16_t dst_port;
    uint16_t size;
    uint16_t seq;
    uint16_t tot;
};

struct MyTCPHeader{
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t size;
    uint8_t type;
    uint16_t slot;
    uint16_t count;
    uint32_t src;
    uint32_t acked_max;
    uint32_t dest;
};



struct MyIPHeader{
    uint8_t type;
    uint8_t TOS;
    uint16_t size;
    uint16_t recognize;
    uint16_t offset;
    uint8_t TTL;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src;
    uint32_t dest;
};

#endif
