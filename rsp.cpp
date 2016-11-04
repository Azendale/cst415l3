// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab4 RSP implementation
// Last modified: 2016-10-31
#include "rsp_if.h"
#include "rsp.h"
#include "queue.h"
#include <pthread.h>
#include <string>
// memset, memcpy
#include <string.h>
#include <iostream>
#include "RspData.h"
#include <map>

// Way high for debugging for now
#define RSP_TIMEOUT 7

static int g_window  = 256;
static pthread_t g_timerThread;
static pthread_t g_readerThread;
// Next 3 lines are so the timer and read functions can blocking wait on a condition variable instead of a read when we have no connections
static bool g_OpenConnections = false;
static pthread_mutex_t g_OpenConnectionsLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_OpenConnectionsCond = PTHREAD_COND_INITIALIZER;
static std::map<std::string, RspData> connections;

using std::string;

void * rsp_timer(void * args)
{
    RspData * conn = static_cast<RspData *>(args);
    while (!Q_Is_Closed(conn->recvq))
    {
        // Wait for timeout period to expire
        struct timespec delay;
        delay.tv_sec = RSP_TIMEOUT / 1000;
        delay.tv_nsec = (RSP_TIMEOUT%1000) * 1000000;
        nanosleep(&delay, NULL);
        
        // Check for timeouts and handle
    }

    return nullptr;
}

void * rsp_reader(void * args)
{
    RspData * conn = static_cast<RspData *>(args);
    rsp_message_t incoming_packet;
    
    bool closed = false;
    while (!closed)
    {
        memset(&incoming_packet, 0, sizeof(incoming_packet));
        int recvCode = rsp_receive(&incoming_packet);
        // Phil says just kill the connection if the return value is not 0
        if (recvCode < 0)
        {
            std::cerr << "Dropped a packet with recvfrom error status " <<  recvCode << std::endl;
            continue;
        }
        // 1 means not enough to have a header
        else if (1 == recvCode)
        {
            std::cerr << "Dropped a packet with less than a header's worth of data.\n";
            continue;
        }
        // 2 means enough for a header, but not necsesarily the payload
        else if (2 == recvCode)
        {
            std::cerr <<  "Warning: received packet with size not matching the payload. Dropping payload.\n";
        }
        // In multi connection support, we would need to check name & ports
        if (incoming_packet.length > 0 && 0 == recvCode)
        {
            rsp_message_t * queuedpacket = new rsp_message_t;
            memcpy(queuedpacket, &incoming_packet, sizeof(rsp_message_t));
            Q_Enqueue(conn->recvq, queuedpacket);
        }
        //TODO: Send acks!
        if (incoming_packet.flags.flags.rst || incoming_packet.flags.flags.err)
        {
            closed = true;
            Q_Close(conn->recvq);
            pthread_mutex_lock(&conn->connection_state_lock);
            conn->connection_state = RSP_STATE_RST;
            pthread_mutex_unlock(&conn->connection_state_lock);
        }
        if (incoming_packet.flags.flags.fin)
        {
            closed = true;
            Q_Close(conn->recvq);
            pthread_mutex_lock(&conn->connection_state_lock);
            conn->connection_state = RSP_STATE_CLOSED;
            pthread_mutex_unlock(&conn->connection_state_lock);
        }
    }
    return nullptr;
}


void rsp_init(int window_size)
{
    g_window = window_size;
    // Need a list of open connections with proper lock here
    // Need to be able to find connection by name
    // Phil says we don't need to differentiate connections with same name and different ports
}

void rsp_shutdown()
{
    // Stop the reader thread
    // Free any resources or locks
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
    // Lock the g_OpenConnections lock
    // check to see if there are any open connections
    // Unlock the lock
    // If we changed it, broadcast/signal
    rsp_message_t request, response;
    RspData * conn = nullptr;
    try
    {
        conn = new RspData;
    }
    catch (std::bad_alloc)
    {
        return nullptr;
    }
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
    if (pthread_create(&(conn->rec_thread), nullptr, rsp_reader, static_cast<void *>(conn)))
    {
        rsp_conn_cleanup(request, conn, true);
        return nullptr;
    } 
    
    return conn;
}

int rsp_close(rsp_connection_t rsp)
{
    RspData * conn = static_cast<RspData *>(rsp);
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
    
    pthread_mutex_lock(&conn->connection_state_lock);
    conn->connection_state = RSP_STATE_CLOSED;
    pthread_mutex_unlock(&conn->connection_state_lock);
    
    bool closeRequestFail = false;
    if (rsp_transmit(&request))
    {
        // Couldn't send fin packet -- but LAB3 doesn't worry about that
        closeRequestFail = true;
    }
    
    // pthread_join receiver thread, which will quit when it sees a fin
    pthread_join(conn->rec_thread, nullptr);
    // Queue should be now closed as recv thread closes it when it exits
    // empty queue, checking each dequeue to see if it errored that the queue is empty
    rsp_message_t * elem = static_cast<rsp_message_t *>(Q_Dequeue_Nowait(conn->recvq));
    while (nullptr != elem)
    {
        delete elem;
        elem = static_cast<rsp_message_t *>(Q_Dequeue_Nowait(conn->recvq));
    }
    
    delete conn;
    
    // Return
    if (closeRequestFail)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int rsp_write(rsp_connection_t rsp, void *buff, int size)
{
    if (size <= 0)
    {
        return -1;
    }
    RspData * conn = static_cast<RspData *>(rsp);
    pthread_mutex_lock(&conn->connection_state_lock);
    if (RSP_STATE_RST == conn->connection_state)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        return -1;
    }
    pthread_mutex_unlock(&conn->connection_state_lock);
    rsp_message_t outgoing_packet;
    memset(&outgoing_packet, 0, sizeof(outgoing_packet));
    
    strncpy(outgoing_packet.connection_name, conn->connection_name.c_str(), RSP_MAX_CONNECTION_NAME_LEN);
    outgoing_packet.src_port = conn->src_port;
    outgoing_packet.dst_port = conn->dst_port;
    outgoing_packet.length = size;
    // Window?
    // LAB3 doesn't need split code but later labs will.
    memcpy(outgoing_packet.buffer, buff, std::min(size, RSP_MAX_SEND_SIZE));
    
    pthread_mutex_lock(&(conn->current_seq_lock));
    outgoing_packet.sequence = htonl(conn->current_seq);
    conn->current_seq += size;
    pthread_mutex_unlock(&(conn->current_seq_lock));
    // No check of return value because LAB3 assumes no dropped packets
    return rsp_transmit(&outgoing_packet);
}

int rsp_read(rsp_connection_t rsp, void *buff, int size)
{
    RspData * conn = static_cast<RspData *>(rsp);
    if (size <= 0)
    {
        return -2;
    }
    pthread_mutex_lock(&conn->connection_state_lock);
    if (RSP_STATE_RST == conn->connection_state)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        return -1;
    }
    pthread_mutex_unlock(&conn->connection_state_lock);
    // Dequeue
    rsp_message_t * incoming;
    incoming = static_cast<rsp_message_t *>(Q_Dequeue(conn->recvq));
    if (nullptr == incoming)
    {
        // Null without blocking means queue is empty
        // which means closed connection. Return 0 to signal that.
        return 0;
    }
    else
    {
        // LAB3 assumes the requested size is the size of the packet,
        // so we will not deal with if they only wanted to take half
        // of the payload from a packet and leave the rest for the
        // next read in this version of the lab.
        int copysize = size;
        if (incoming->length < copysize)
        {
            copysize = incoming->length;
        }
        memcpy(buff, incoming->buffer, copysize);
        delete incoming;
        return copysize;
    }
}

