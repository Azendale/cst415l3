// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab4 testing program (test 2)
// Last modified: 2016-11-14
//
// Run this test 3 times, followed by clearconn
// Third time this is run, it should test what happens when you get an RST on connect
// Use it to test like this (this is what a passing test should look like,
// failing test would say it got a non-null rsp for the third time or would block.)
// [erik.andersen@loki cst415l3]$ ./t_rsp_2
// If this is the first run, ctrl-c when you see this message.
// ^C
// [erik.andersen@loki cst415l3]$ ./t_rsp_2
// If this is the first run, ctrl-c when you see this message.
// Got non-null rsp object.
// [erik.andersen@loki cst415l3]$ ./t_rsp_2
// If this is the first run, ctrl-c when you see this message.
// Got null rsp object.
// [erik.andersen@loki cst415l3]$ ./clearconn erik_rsp_client
// Connection erik_rsp_client cleared

#include <string.h>
#include <stdio.h>

#include "rsp.h"

int main(int argc, char **argv)
{
    rsp_init(2000);

    rsp_connection_t rsp;

    printf("If this is the first run, ctrl-c when you see this message.\n");
    rsp = rsp_connect("erik_rsp_client");

    if (rsp != NULL) 
    {
        printf("Got non-null rsp object.\n");
        rsp_close();
    }
    else
    {
        printf("Got null rsp object.\n");
    }
    rsp_shutdown();

    return 0;
}
