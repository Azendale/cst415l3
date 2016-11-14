// Written by Phil Howard <phil.howard@oit.edu>
// Small modifications by Erik Andersen <erik@eoni.com> (change connection string)
// Last modified: 2016-10-31
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
        rsp_read(rsp, buff, sizeof(buff));
        printf("Received: %s\n", buff);

        rsp_close(rsp);
    }
    rsp_shutdown();

    return 0;
}
