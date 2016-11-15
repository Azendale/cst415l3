// Written by Phil Howard <phil.howard@oit.edu>
// Small modifications by Erik Andersen <erik@eoni.com> (change connection string, add rsp_shutdown)
// Last modified: 2016-11-14
#include <string.h>
#include <stdio.h>

#include "rsp.h"

int main(int argc, char **argv)
{
    char buff[200];
    int length;
    int total_bytes = 0;

    if (argc > 1) strcat(buff, argv[1]);
    strcat(buff, "\n");

    rsp_init(2000);

    rsp_connection_t rsp;

    rsp = rsp_connect("eba1_xfer_client");
    if (rsp != NULL) 
    {
        printf("Connection established\n");
        //while (fgets(buff, sizeof(buff), stdin) != NULL)
        while ((length = rsp_read(rsp, buff, sizeof(buff))) > 0)
        {
            fwrite(buff, 1, length, stdout);
            total_bytes += length;
        }

        rsp_close(rsp);
        printf("received %d bytes\n", total_bytes);
    }
    rsp_shutdown();

    return 0;
}
