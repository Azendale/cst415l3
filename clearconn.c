#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsp_if.h"

int main(int argc, char **argv)
{
    rsp_message_t msg;
    int status;

    if (argc < 2) 
    {
        fprintf(stderr, "clearport <name>\n");
        exit(1);
    }

    memset(&msg, 0, sizeof(msg));
    strcpy(msg.connection_name, argv[1]);
    msg.flags.flags.fin = 1;
    msg.flags.flags.rst = 1;

    status = rsp_transmit(&msg);
    if (status != 0)
    {
        fprintf(stderr, "Unable to send to rsp network\n");
        return 1;
    }

    status = rsp_receive(&msg);
    if (status != 0)
    {
        fprintf(stderr, "Unable to receive from rsp network\n");
        return 2;
    }

    if (msg.flags.flags.fin && msg.flags.flags.rst && msg.flags.flags.ack)
    {
        printf("Connection %s cleared\n", argv[1]);
        return 0;
    }
    else
    {
        printf("Error clearing connection %s\n", argv[1]);
    }

    return 3;
}
