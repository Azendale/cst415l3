#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "nameserver.h"
#include "getport.h"

static int g_service_port = 35000;
static char g_hostname[512] = "unix.cset.oit.edu";
static int g_nameserver = -1;
static struct sockaddr_in g_server_addr;

//*************************************************
void Fatal_Error(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);

    vfprintf(stderr, msg, args);

    va_end(args);

    exit(-1);
}
//*************************************************
int setup_ns(const char *host, int port)
{
    // see if we've already done this
    if (g_nameserver >= 0) return 0;

    if (host != NULL) strcpy(g_hostname, host);
    if (port > 0) g_service_port = port;

    g_nameserver = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_nameserver < 0) Fatal_Error("Unable to create service socket");

    struct sockaddr_in myaddr;
    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(0);        // request a port

    if (bind(g_nameserver, (struct sockaddr *)&myaddr, sizeof(myaddr)) != 0)
    {
        Fatal_Error("Unable to bind to port for communicating with ns\n");
    }

    struct addrinfo hints;
    struct addrinfo *addr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    if (getaddrinfo(g_hostname, NULL, &hints, &addr) != 0)
    {
        perror("Error getting address info: ");
        Fatal_Error("Error getting address info\n");
    }

    memcpy(&g_server_addr, addr->ai_addr, addr->ai_addrlen);
    g_server_addr.sin_port = htons(g_service_port);

    freeaddrinfo(addr);

    return 0;
}
//*************************************************
static int do_request(const char *service_name, int type, int port)
{
    request_t request;
    request_t result;
    int status;
    int recvlen;

    setup_ns(NULL, 0);

    memset(&request, 0, sizeof(request));
    request.msg_type = type;
    strcpy(request.service_name, service_name);
    request.port = htons(port);
    request.status = SUCCESS;

    status = sendto(g_nameserver, &request, sizeof(request), 0, 
        (struct sockaddr *)&g_server_addr, sizeof(g_server_addr));
    if (status < 0) 
    {
        //perror("Error sending msg: ");
        return -1;
    }

    recvlen = recvfrom(g_nameserver, &result, sizeof(result), 0, NULL, NULL);
    if (recvlen < 0) 
    {
        //Fatal_Error("Error on service socket");
        return -2;
    }

    if (recvlen != sizeof(request)) 
    {
        //fprintf(stderr, "Received an unrecognized packet\n");
        return -3;
    }

    if (result.status != SUCCESS)
    {
        fprintf(stderr, "Request failure: %d\n", result.status);
        return -4;
    }

    return ntohs(result.port);
}
//*************************************************
int request_port(const char *service_name)
{
    return do_request(service_name, REQUEST_PORT, 0);
}
//*************************************************
int lookup_port(const char *service_name)
{
    return do_request(service_name, LOOKUP_PORT, 0);
}
//*************************************************
int release_port(const char *service_name, int port)
{
    return do_request(service_name, CLOSE_PORT, port);
}
//*************************************************
int keep_alive_port(const char *service_name, int port)
{
    return do_request(service_name, KEEP_ALIVE, port);
}
