// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab3 testing program (test 3, part b)
// Last modified: 2016-10-31
//
// Tests what happens when an RST happens after connecting
// Use: Start the other test3 part first. (./t_rsp_3_rst_inject)
// Then start this program.
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
        printf("Pausing until newline.\n");
        char line[256];
        fgets(line, sizeof(line), stdin);
        printf("Value of rsp_write after trying to write to RST'd connection: %d\n", rsp_write(rsp, buff, strlen(buff)+1));
        memset(buff, 0, sizeof(buff));
        printf("Value of rsp_read after trying to read from RST'd connection: %d\n", rsp_read(rsp, buff, sizeof(buff)));
        rsp_close(rsp);
    }

    return 0;
}
