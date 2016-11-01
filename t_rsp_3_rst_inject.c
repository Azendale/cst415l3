// Attemps to inject an RST packet into a connnection right after it starts
#include <string.h>
#include <stdio.h>

#include "rsp.h"
#include "rsp_if.h"

int main(int argc, char **argv)
{
    rsp_init(2000);

    rsp_connection_t rsp;

    rsp = rsp_connect("erik_rsp_client");

    if (rsp != NULL) 
    {
        printf("Connection established\n");
        printf("Attempting to inject an RST.\n");
        rsp_message_t rstPacket;
        memset(&rstPacket, 0, sizeof(rstPacket));
        strncpy(rstPacket.connection_name, "erik_rsp_client", RSP_MAX_CONNECTION_NAME_LEN);
        rstPacket.src_port = 1;
        rstPacket.dst_port = 2;
        rstPacket.flags.flags.rst = 1;
        rsp_transmit(&rstPacket);

    }

    return 0;
}
