#include "rsp_if.h"
#include "rsp.h"
#include <pthread.h>
// memset, memcpy
#include <string.h>

class RspData
{
    // List for each piece of state what threads read, write it, and decide if lock is needed.
    pthread_t rec_thread;
    uint16_t src_port;
    uint16_t dst_port;
};

void * rsp_reader(void * args)
{
    RspData * conn = reinterpret_cast<RspData *>(args);
    
    
    // If fin packet, close queue
}


void rsp_init(int window_size)
{
}

rsp_connection_t rsp_connect(const char *connection_name)
{
    rsp_message_t request, response;
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    
    // Send connection request
    
    // Read (block) for SYNACK to connection
    
    // Parse src and dest port, save them
    // Spin off read thread
    return nullptr;
}

int rsp_close(rsp_connection_t rsp)
{
    RspData * conn = reinterpret_cast<RspData *>(rsp);
    // State to close_wait
    // Send fin
    // empty queue, checking each dequeue to see if it errored that the queue is empty
    // state to closed
    // Return
    return 0;
}

int rsp_write(rsp_connection_t rsp, void *buff, int size)
{
    RspData * conn = reinterpret_cast<RspData *>(rsp);
    // lock state
    // check that it is established, not closing
    // unlock?
    // send packet
    // unlock?
    return 0;
}

int rsp_read(rsp_connection_t rsp, void *buff, int size)
{
    RspData * conn = reinterpret_cast<RspData *>(rsp);
    // Dequeue
    // write to buf
    return 0;
}

