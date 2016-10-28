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
                delete reinterpret_cast<rsp_message_t *>(item);
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
    uint64_t far_first_sequence;
    uint64_t our_first_sequence;
    uint16_t far_window;
    
    string connection_name;
    queue_t recvq;
};

void * rsp_reader(void * args)
{
    RspData * conn = reinterpret_cast<RspData *>(args);
    
    
    // If fin packet, close queue
    // Quit after fin packet
    return nullptr;
}


void rsp_init(int window_size)
{
}

static void rsp_conn_cleanup(rsp_message_t & request, RspData * & conn, bool rst_far_end)
{
    if (rst_far_end)
    {
            memset(&request, 0, sizeof(request));
            request.src_port = htons(conn->src_port);
            request.dst_port = htons(conn->dst_port);
            strncpy(request.connection_name, conn->connection_name.c_str(), RSP_MAX_CONNECTION_NAME_LEN);
            request.flags.flags.rst = 1;
            rsp_transmit(&request);       
    }
    if (conn)
    {
        delete conn;
        conn = nullptr;
    }
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
    
    if (response.flags.flags.rst)
    {
        rsp_conn_cleanup(request, conn, false);
        return nullptr;
    }
    if (! (response.flags.flags.ack && response.flags.flags.syn))
    {
        rsp_conn_cleanup(request, conn, true);
        return nullptr;
    }
    
    // Parse src and dest port, save them
    conn->src_port = response.src_port;
    conn->dst_port = response.dst_port;
    
    // Null terminate the name of the string, just in case it is not
    response.connection_name[RSP_MAX_CONNECTION_NAME_LEN] = '\0';
    conn->connection_name = string(response.connection_name);
    
    conn->our_first_sequence = 0;
    conn->far_first_sequence = ntohl(response.sequence);
    conn->far_window = ntohs(response.window);
    
    // Spin off read thread
    if (!pthread_create(&(conn->rec_thread), nullptr, rsp_reader, reinterpret_cast<void *>(conn)))
    {
        rsp_conn_cleanup(request, conn, true);
        return nullptr;
    } 
    
    return conn;
}

int rsp_close(rsp_connection_t rsp)
{
    RspData * conn = reinterpret_cast<RspData *>(rsp);
    // State to close_wait (do we even need state for LAB3 or queue enough)
    
    // Send fin
    rsp_message_t request;
    memset(&request, 0, sizeof(request));
    strncpy(request.connection_name, conn->connection_name.c_str(), RSP_MAX_CONNECTION_NAME_LEN);
    request.src_port = conn->src_port;
    request.dst_port = conn->dst_port;
    request.flags.flags.fin = 1;
    // length is already 0 from memset
    request.window = htons(g_window);
    // sequence doesn't make sense if there is no data, right?
    // ack sequence doesn't make sense, we aren't acking anything here
    // buffer has no data
    
    if (rsp_transmit(&request))
    {
        // Couldn't send fin packet -- what to do?
        
    }
    
    // pthread_join receiver thread, which will quit when it sees a fin
    pthread_join(conn->rec_thread, nullptr);
    // Queue should be now closed as recv thread closes it when it exits
    // empty queue, checking each dequeue to see if it errored that the queue is empty
    rsp_message_t * elem = reinterpret_cast<rsp_message_t *>(Q_Dequeue_Nowait(conn->recvq));
    while (nullptr != elem)
    {
        delete elem;
        elem = reinterpret_cast<rsp_message_t *>(Q_Dequeue_Nowait(conn->recvq));
    }
    
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

