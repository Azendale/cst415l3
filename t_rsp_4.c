#include <string.h>
#include <stdio.h>

#include "rsp.h"
#include "rsp_if.h"

int main(int argc, char **argv)
{
    rsp_init(2000);
    char buff[200] = "Data sent to connection 1";
    char buff2[200] = "Data sent to connection 2";
    char buff3[200] = "";

    strcat(buff, "\n");
    strcat(buff2, "\n");

    rsp_connection_t rsp;
    rsp_connection_t rsp2;

    rsp = rsp_connect("erik_multi_rsp_client");
    if (rsp != NULL) 
    {
        printf("Connection 1 established\n");
    }
    else
    {
        printf("Couldn't connect for connection 1.\n");
        return -1;
    }
    rsp2 = rsp_connect("erik_multi_rsp_client2");

    if (rsp != NULL) 
    {
        printf("Connection 2 established\n");
        printf("Write to connection 1");
        printf("Read from connection 2");
        memset(buff3, 0, sizeof(buff3));
        rsp_read(rsp2, buff3, sizeof(buff3));
        printf("%s\n", buff3);
        printf("Write to connection 2");
        printf("Read from connection 1");
        memset(buff3, 0, sizeof(buff3));
        rsp_read(rsp, buff3, sizeof(buff3));
        printf("%s\n", buff3);
        rsp_close(rsp);
        rsp_close(rsp2);
    }
    rsp_shutdown();

    return 0;
}
