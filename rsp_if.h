#pragma once
//*******************************************
// Header file for RSP: the Reliable Stream Protocol
//
// Author: Phil Howard
//
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RSP_MAX_CONNECTION_NAME_LEN 31
#define RSP_MAX_SEND_SIZE           256

typedef struct
{
    uint8_t syn:1;
    uint8_t ack:1;
    uint8_t fin:1;
    uint8_t rst:1;
    uint8_t err:1;
    uint8_t nod:1;
    uint8_t nro:1;
    uint8_t reserved:1;
} rsp_flags_t;

typedef struct
{
    char connection_name[RSP_MAX_CONNECTION_NAME_LEN + 1];
    uint16_t src_port;
    uint16_t dst_port;
    union
    {
        uint8_t all_flags;
        rsp_flags_t flags;
    } flags;
    uint8_t length;
    uint16_t window;
    uint64_t sequence;
    uint64_t ack_sequence;
    char buffer[RSP_MAX_SEND_SIZE];
} rsp_message_t;

int rsp_transmit(rsp_message_t *msg);
int rsp_receive(rsp_message_t *msg);

#ifdef __cplusplus
}
#endif

