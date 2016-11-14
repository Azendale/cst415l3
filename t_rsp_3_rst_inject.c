// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab3 testing program (test 3, part a)
// Last modified: 2016-10-31
//
// Attemps to inject an RST packet into a connnection right after it starts

// Use: start this part of test3 first.
// Then start the other part3 test program, and hit enter.
// It should print something like this:
//
//Connection established
//Pausing until newline.
//
//Value of rsp_write after trying to write to RST'd connection: -1
//Value of rsp_read after trying to read from RST'd connection: -1

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
    rsp_shutdown();

    return 0;
}
