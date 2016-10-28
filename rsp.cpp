#include "rsp_if.h"
#include "rsp.h"
#include "queue.h"
#include <pthread.h>
#include <string>
// memset, memcpy
#include <string.h>

static int g_window  = 0;
using std::string;

class RspData
{
public:
    RspData()
    {
        recvq = Q_Init();
        if (nullptr == recvq)
        {
            throw std::bad_alloc();
        }
    }
    ~RspData()
    {
        if (recvq)
        {
            // Under what senarios can this fail? Unless pthread_cond_broadcast fails, I don't see how it would
            Q_Close(recvq);
            void * item;
            while (nullptr != (item = Q_Dequeue(recvq)))
            {
                delete item;
            }
            Q_Destroy(recvq);
            recvq = nullptr;
        }
    }
    
    
    // List for each piece of state what threads read, write it, and decide if lock is needed.
    pthread_t rec_thread;
    // Set by rsp_connect, read by write and close, no lock needed
    // We plan to do no math on the following two variables, so by convention, they will be in network order
    uint16_t src_port;
    uint16_t dst_port;
    // These are in HOST order, as they are going to be used in calculations.
    uint64_t sequence;
    uint64_t ack_sequence;
    int farWindow;
    
    string connection_name;
    queue_t recvq;
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
    
    RspData * conn = new RspData;
    if (nullptr == conn)
    {
        return nullptr;
    }
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    
    // Send connection request
    // fill out struct
    //connection_name[RSP_MAX_CONNECTION_NAME_LEN + 1]
    strncpy(request.connection_name, connection_name, RSP_MAX_CONNECTION_NAME_LEN);
    // src_port, dst_port already 0 from memset
    // flags, 0 by default from memset
    request.flags.flags.syn = 1;
    // length -- 0 from memset, no payload on this syn, so already correct
    // window
    request.window = htons(g_window);
    // sequence -- not doing acks yet, but seems like an OK place to start
    // ack_sequence, not doing acks yet, nothing to ack on initial connect anyway
    // buffer[RSP_MAX_SEND_SIZE] -- no payload, empty
    
    // LAB3 assumes no drops, so not checking return value here for now
    rsp_transmit(&request);
    
    // Read (block) for SYNACK to connection
    // LAB3 assumes no drops, so not checking return value here for now
    rsp_receive(&response);
    
    if (! response.flags.flags.ack || response.flags.flags.rst)
    {
        delete conn;
        return nullptr;
    }
    
    // Parse src and dest port, save them
    conn->src_port = response.src_port;
    conn->dst_port = response.dst_port;
    
    // Null terminate the name of the string, just in case it is not
    response.connection_name[RSP_MAX_CONNECTION_NAME_LEN] = '\0';
    conn->connection_name = string(response.connection_name);
    
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

