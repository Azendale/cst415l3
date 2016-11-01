// Run this test 3 times, followed by clearconn
// Second time this is run, it should test what happens when you get an RST on connect
#include <string.h>
#include <stdio.h>

#include "rsp.h"

int main(int argc, char **argv)
{
    rsp_init(2000);

    rsp_connection_t rsp;

    printf("If this is the first or second run, ctrl-c when you see this message.\n");
    rsp = rsp_connect("erik_rsp_client");

    if (rsp != NULL) 
    {
        printf("Got non-null rsp object.\n");
    }
    else
    {
        printf("Got null rsp object.\n");
    }

    return 0;
}
