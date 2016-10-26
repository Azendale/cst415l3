#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void * rsp_connection_t;

void rsp_init(int window_size);
rsp_connection_t rsp_connect(const char *connection_name);
int rsp_close(rsp_connection_t rsp);
int rsp_write(rsp_connection_t rsp, void *buff, int size);
int rsp_read(rsp_connection_t rsp, void *buff, int size);

#ifdef __cplusplus
}
#endif

