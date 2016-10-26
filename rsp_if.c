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
#include <errno.h>

#include "rsp_if.h"
#include "getport.h"

static int g_server_port;
static struct sockaddr_in g_server_addr;
static int g_server_fd = -1;
static char g_hostname[256] = "unix.cset.oit.edu";

static const int HEADER_SIZE = sizeof(rsp_message_t) - RSP_MAX_SEND_SIZE;

//***********************************
//*********** Contains race *********
//***********************************
static void init_socket()
{
    if (g_server_fd < 0)
    {
        g_server_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (g_server_fd < 0)
        {
            perror("Unable to create socket for server\n");
            exit(1);
        }

        g_server_port = lookup_port("rsp_server");
        if (g_server_port < 0)
        {
            fprintf(stderr, "Unable to communicate with name server: %d\n",
                    g_server_port);
            exit(1);
        }

        struct sockaddr_in myaddr;
        memset((char *)&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        myaddr.sin_port = htons(0);

        if (bind(g_server_fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) != 0)
        {
            perror("Unable to bind to socket\n");
            exit(1);
        }

        struct addrinfo hints;
        struct addrinfo *addr;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;

        if (getaddrinfo(g_hostname, NULL, &hints, &addr) != 0)
        {
            perror("Error getting address info: ");
            exit(1);
        }

        memcpy(&g_server_addr, addr->ai_addr, addr->ai_addrlen);
        g_server_addr.sin_port = htons(g_server_port);

        freeaddrinfo(addr);
    }
}
//*********************************************************
int rsp_transmit(rsp_message_t *msg)
{
    int status;

    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = (HEADER_SIZE + msg->length)*8*1000;
    nanosleep(&delay, NULL);

    init_socket();

    do
    {
        status = sendto(g_server_fd, msg, HEADER_SIZE + msg->length, 0,
                (struct sockaddr *)&g_server_addr, sizeof(g_server_addr));
    } while (status < 0 && errno==EINTR);

    if (status < 0) return status;
    if (status < HEADER_SIZE) return 1;
    if (status != HEADER_SIZE + msg->length) return 2;
    return 0;
}
//*********************************************************
int rsp_receive(rsp_message_t *msg)
{
    int status;

    init_socket();

    do
    {
        status = recvfrom(g_server_fd, msg, sizeof(rsp_message_t), 
                0, NULL, NULL);
    } while (status < 0 && errno==EINTR);

    if (status < 0) return status;
    if (status < HEADER_SIZE) return 1;
    if (status != HEADER_SIZE + msg->length) return 2;

    return 0;
}

