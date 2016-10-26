#include <stdio.h>
#include <stdlib.h>

#include "getport.h"

int main(int argc, char **argv)
{
    if (argc < 3) Fatal_Error("clearport <name> <port>\n");
    int port;
    int result;

    port = atoi(argv[2]);
    result = release_port(argv[1], port);

    printf("Release: %d\n", result);

    return 0;
}
