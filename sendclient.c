#include <string.h>
#include <stdio.h>

#include "rsp.h"

int main(int argc, char **argv)
{
    char buff[200] = "Hello world ";
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
        while ((length = fread(buff, 1, sizeof(buff), stdin)) != 0)
        {
            fwrite(buff, length, 1, stdout);
            rsp_write(rsp, buff, length);
            total_bytes += length;
        }

        rsp_close(rsp);
        printf("sent %d bytes\n", total_bytes);
    }

    return 0;
}
