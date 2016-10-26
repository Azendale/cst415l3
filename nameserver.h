#pragma once

//*******************************************************
// nameservice.h
//
// Type definitions for CST 415 Name Service Server interface
//
// Author: Philip Howard phil.howard@oit.edu
//
// Date: 9/27/2016
//

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SERVICE_NAME_LEN 49

typedef struct
{
    int8_t msg_type;
    char service_name[MAX_SERVICE_NAME_LEN + 1];
    uint16_t port;
    int8_t status;
} request_t;

// Valid msg_type values
#define REQUEST_PORT 1
#define LOOKUP_PORT  2
#define KEEP_ALIVE   3
#define CLOSE_PORT   4
#define RESPONSE     5
#define PORT_DEAD    6
#define STOP         7

// Valid status values
#define SUCCESS           0
#define SERVICE_IN_USE    1
#define SERVICE_NOT_FOUND 2
#define ALL_PORTS_BUSY    3
#define INVALID_ARG       4
#define UNDEFINED_ERROR   5

#ifdef __cplusplus
}
#endif

