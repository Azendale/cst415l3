#include "rsp_if.h"
#include "rsp.h"

void rsp_init(int window_size)
{
}

rsp_connection_t rsp_connect(const char *connection_name)
{
    return nullptr;
}

int rsp_close(rsp_connection_t rsp)
{
    return 0;
}

int rsp_write(rsp_connection_t rsp, void *buff, int size)
{
    return 0;
}

int rsp_read(rsp_connection_t rsp, void *buff, int size)
{
    return 0;
}

