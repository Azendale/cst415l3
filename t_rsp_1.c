// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab3 testing program (test 1)
// Last modified: 2016-10-31
//
// Test that nothing goes wrong when you close a connection with data left in the buffer.
// Should output something like:
// rsp_close value after closing with remaining data in buffer: 0
#include <string.h>
#include <stdio.h>

#include "rsp.h"

int main(int argc, char **argv)
{
    char buff[200] = "Hello world ";

    if (argc > 1) strcat(buff, argv[1]);
    strcat(buff, "\n");

    rsp_init(2000);

    rsp_connection_t rsp;

    rsp = rsp_connect("erik_rsp_client");

    if (rsp != NULL) 
    {
        printf("Connection established\n");
        rsp_write(rsp, buff, strlen(buff)+1);
        memset(buff, 0, sizeof(buff));

        printf("rsp_close value after closing with remaining data in buffer: %d\n", rsp_close(rsp));
    }
    rsp_shutdown();

    return 0;
}
