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
#include <sys/time.h>

// Way high for debugging for now
#define RSP_TIMEOUT 7000

static int g_window  = 256;
static pthread_t g_readerThread;
// Next 3 lines are so the timer and read functions can blocking wait on a condition variable instead of a read when we have no connections
// Who can close a connection: Reader thread, or a close call from main thread
static bool g_readerContinue = true;
static pthread_mutex_t g_connectionsLock = PTHREAD_MUTEX_INITIALIZER;
static std::map<std::string, RspData *> g_connections;

using std::string;

// Returns a milliseconds since epoch timestamp
static uint64_t timestamp()
{
    struct timeval time;
    uint64_t timestamp;

    gettimeofday(&time, NULL);

    timestamp = time.tv_sec;
    timestamp *= 1000;
    timestamp += time.tv_usec/1000;

    return timestamp;
}

void sleepRspTimeout()
{
    struct timespec delay;
    delay.tv_sec = RSP_TIMEOUT / 1000;
    delay.tv_nsec = (RSP_TIMEOUT%1000) * 1000000;
    nanosleep(&delay, NULL);
}

// Figure out how much more time we need to wait for RSP_TIMEOUT amount of time to have passed
uint64_t expireDelay(uint64_t pastTimestamp)
{
    uint64_t pastOffset = timestamp() - pastTimestamp;
    if (pastOffset >= RSP_TIMEOUT)
    {
        return 0;
    }
    else
    {
        return  RSP_TIMEOUT - pastOffset;
    }
}

// Pause for a certain number of milliseconds.
void sleepMilliseconds(uint64_t msdelay)
{
    struct timespec delay;
    delay.tv_sec =  msdelay / 1000;
    delay.tv_nsec = (msdelay%1000) * 1000000;
    nanosleep(&delay, NULL);
}

// Figure out how long we need to wait for the packet in the front of the ackq to timeout, and 
// store the amount of time to wait in delay. Store the end of the byte range the packet completes
// in sequenceTotal
static void getNextAckqPacketDelay(RspData * conn, uint64_t & delay, int64_t & sequenceTotal)
{
    if (conn->ackq.empty())
    {
        delay = RSP_TIMEOUT;
        sequenceTotal = -1;
    }
    else
    {
        ackq_entry_t & waitingPacket = conn->ackq.front();
        delay = expireDelay(waitingPacket.lastSent);
        sequenceTotal = ntohl(waitingPacket.packet.sequence) + waitingPacket.packet.length;
    }
}

static void prepare_outgoing_packet(RspData & conn, rsp_message_t & packet)
{
    memset(&packet, 0, sizeof(packet));
    strncpy(packet.connection_name, conn.connection_name.c_str(), RSP_MAX_CONNECTION_NAME_LEN);
    packet.src_port = conn.src_port;
    packet.dst_port = conn.dst_port;
    packet.window = htons(g_window);
    // Not totally sure this shouldn't be set by the calling function. We'll see
    packet.sequence = htonl(conn.current_seq);
}

// Send a packet that needs to be acked, automatically placing it in the ackq
static int sendPacket(rsp_connection_t conn, rsp_message_t & packet, uint8_t timesSentSoFar)
{
    ackq_entry_t ackqEntry;
    ackqEntry.packet = packet;
    ackqEntry.lastSent = timestamp();
    ackqEntry.sendCount = timesSentSoFar + 1;
    static_cast<RspData *>(conn)->ackq.push_back(ackqEntry);
    return rsp_transmit(&packet);
}

// retransmit lost packet -- retransmits the packet on the top of the ackq
// returns false if this is the third time or we weren't able to send
// precondition: there must actually be a packet in the head of the ackq
static bool retransmitHeadPacket(rsp_connection_t conn)
{
    ackq_entry_t & lostPacket = static_cast<RspData *>(conn)->ackq.front();
    if (lostPacket.sendCount <= 3)
    {
        lostPacket.lastSent = timestamp();
        lostPacket.sendCount += 1;
        return ! rsp_transmit(&lostPacket.packet);
    }
    else
    {
        return false;
    }
}

// One timer thread per connection
void * rsp_timer(void * args)
{
    RspData * conn = static_cast<RspData *>(args);
    uint64_t expireDelay;
    // 64 bits instead of 32 because I need another bit for signedness (negative means "null" sequence number)
    int64_t sequenceTotal;

    pthread_mutex_lock(&conn->connection_state_lock);
    getNextAckqPacketDelay(conn, expireDelay, sequenceTotal);
    
    while (RSP_STATE_OPEN == conn->connection_state || RSP_STATE_WECLOSED == conn->connection_state)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        sleepMilliseconds(expireDelay);
        
        pthread_mutex_lock(&conn->connection_state_lock);
        // Is there a packet we were waiting for?
        if (0 <= sequenceTotal)
        {
            // If so, did it timeout while we were asleep? (if the queue is not empty and it's the same packet at the front)
            if ( (!conn->ackq.empty()) && ntohl(conn->ackq.front().packet.sequence) + conn->ackq.front().packet.length == sequenceTotal)
            {
                // packet was not acked, it is the first in the queue
                // Returns false if this is more than the third time or we fail to transmit
                if (!retransmitHeadPacket(conn))
                {
                    conn->connection_state = RSP_STATE_RST;
                    pthread_cond_broadcast(&conn->connection_state_cond);
                    pthread_mutex_unlock(&conn->connection_state_lock);
                    return nullptr;
                }
            }
        }
        
        getNextAckqPacketDelay(conn, expireDelay, sequenceTotal);
    }
    pthread_mutex_unlock(&conn->connection_state_lock);
    
    return nullptr;
}

void * rsp_reader(void * args)
{
    rsp_message_t incoming_packet;
    bool readCont = true;
    
    while (readCont)
    {
        // Block for a read
        memset(&incoming_packet, 0, sizeof(incoming_packet));
        rsp_receive(&incoming_packet);
        // decide what connection
        // Ensure null truncation
        incoming_packet.connection_name[RSP_MAX_CONNECTION_NAME_LEN] = '\0';
        std::string connName = std::string(incoming_packet.connection_name);
        // Find thread by name
        pthread_mutex_lock(&g_connectionsLock);
        readCont = g_readerContinue;
        auto it = g_connections.find(connName);
        if (g_connections.cend() == it)
        {
            // Couldn't find matching connection
            std::cerr << "Got a packet for connection name " << connName << " but there is no active connection by that name." << std::endl;
            pthread_mutex_unlock(&g_connectionsLock);
            continue;
        }
        // Handle packet
        pthread_mutex_lock(&it->second->connection_state_lock);
        
        // Is packet RST
        if (incoming_packet.flags.flags.rst || incoming_packet.flags.flags.err)
        {
            it->second->connection_state = RSP_STATE_RST;
            pthread_cond_broadcast(&it->second->connection_state_cond);
            
            pthread_mutex_unlock(&it->second->connection_state_lock);
            // Remove from list of connections
            g_connections.erase(it);
            pthread_mutex_unlock(&g_connectionsLock);
            continue;
        }
        // Is packet SYN + ACK
        else if (incoming_packet.flags.flags.syn && incoming_packet.flags.flags.ack)
        {
            // SYNACK
            // Parse src and dest port, save them
            it->second->src_port = incoming_packet.src_port;
            it->second->dst_port = incoming_packet.dst_port;
            
            // Null terminate the name of the string, just in case it is not
            incoming_packet.connection_name[RSP_MAX_CONNECTION_NAME_LEN] = '\0';
            it->second->connection_name = string(incoming_packet.connection_name);
            
            //it->second->far_window = ntohs(incoming_packet.window);
            it->second->ack_highwater = ntohl(incoming_packet.sequence) + incoming_packet.length;
            
            it->second->connection_state = RSP_STATE_OPEN;
            pthread_cond_broadcast(&it->second->connection_state_cond);
            pthread_mutex_unlock(&g_connectionsLock);
            pthread_mutex_unlock(&it->second->connection_state_lock);
            continue;
        }
        
        // take stuff out of the timeout queue when it is acked
        if (incoming_packet.flags.flags.ack)
        {
            uint32_t receivedThru = ntohl(incoming_packet.ack_sequence);
            while (! it->second->ackq.empty() && ntohl(it->second->ackq.front().packet.sequence) + it->second->ackq.front().packet.length <= receivedThru)
            {
                it->second->ackq.pop_front();
            }
        }
       
        // if packet's byte range does not start at ack_highwater, filter it out (drop)
        if (it->second->ack_highwater != ntohl(incoming_packet.sequence))
        {
            // do nothing/continue loop -- we're dropping this packet
            pthread_mutex_unlock(&g_connectionsLock);
            pthread_mutex_unlock(&it->second->connection_state_lock);
            continue;
        }
        
        it->second->ack_highwater = ntohl(incoming_packet.sequence) + incoming_packet.length;
        
        // Is packet FIN
        if (incoming_packet.flags.flags.fin)
        {
            // No more packets expected from them
            Q_Close(it->second->recvq);
            if (RSP_STATE_WECLOSED == it->second->connection_state)
            {
                // Connection now full closed
                it->second->connection_state = RSP_STATE_CLOSED;
            }
            else
            {
                it->second->connection_state = RSP_STATE_THEYCLOSED;
                // Do we have a premade function to send fin with?
                rsp_message_t lastFin;
                prepare_outgoing_packet(*it->second, lastFin);
                lastFin.flags.flags.fin = 1;
                lastFin.ack_sequence = htonl(it->second->ack_highwater);
                rsp_transmit(&lastFin);
                it->second->connection_state = RSP_STATE_CLOSED;
            }
            pthread_cond_broadcast(&it->second->connection_state_cond);
            pthread_mutex_unlock(&it->second->connection_state_lock);
            g_connections.erase(it);
            pthread_mutex_unlock(&g_connectionsLock);
            continue;
        }
        pthread_mutex_unlock(&g_connectionsLock);
        
        rsp_message_t ackPacket;
        ackPacket.ack_sequence = htonl(it->second->ack_highwater);
        ackPacket.flags.flags.ack = 1;
        // send ack
        prepare_outgoing_packet(*it->second, ackPacket);
        rsp_transmit(&ackPacket);
        
        // if we have any payload
        if (0 < incoming_packet.length)
        {
            rsp_message_t * queuedpacket = new rsp_message_t;
            memcpy(queuedpacket, &incoming_packet, sizeof(rsp_message_t));
            Q_Enqueue(it->second->recvq, queuedpacket);
        }
        pthread_mutex_unlock(&it->second->connection_state_lock);
    }
    
    return nullptr;
}


void rsp_init(int window_size)
{
    g_window = window_size;
    // Phil says we don't need to differentiate connections with same name and different ports
    g_readerContinue = true;
    
    // Spin off read thread
    pthread_create(&(g_readerThread), nullptr, rsp_reader, nullptr);
 }

// Precondition: You must have cleaned up all connections (closed them)
void rsp_shutdown()
{
    // Stop the reader thread
    pthread_mutex_lock(&g_connectionsLock);
    g_readerContinue = false;
    pthread_mutex_unlock(&g_connectionsLock);
    // Free any resources or locks
    
    // Break reader thread out of it's wait by getting the server to echo anything back at us
    rsp_message_t reflection;
    memset(&reflection, 0, sizeof(reflection));
    rsp_transmit(&reflection);
    
    pthread_join(g_readerThread, nullptr);
}

// Cleanup for the rsp_connect() function.
static void rsp_connect_cleanup(RspData * & conn, bool rst_far_end)
{
    if (rst_far_end)
    {
        rsp_message_t request;
        memset(&request, 0, sizeof(request));
        request.src_port = htons(conn->src_port);
        request.dst_port = htons(conn->dst_port);
        strncpy(request.connection_name, conn->connection_name.c_str(), RSP_MAX_CONNECTION_NAME_LEN);
        request.flags.flags.rst = 1;
        rsp_transmit(&request);       
    }
    // Must release lock first to ensure we can avoid deadlock
    pthread_mutex_unlock(&conn->connection_state_lock);
    
    // Ensure the connection is removed from the global list
    pthread_mutex_lock(&g_connectionsLock);
    auto it = g_connections.find(conn->connection_name);
    if (g_connections.end() != it)
    {
        g_connections.erase(it);
    }
    
    pthread_mutex_lock(&conn->connection_state_lock);
    pthread_mutex_unlock(&g_connectionsLock);
    
    // Now that the receive thread can't access the connection, clean it up
    pthread_mutex_unlock(&conn->connection_state_lock);
    if (conn)
    {
        delete conn;
        conn = nullptr;
    }
}

rsp_connection_t rsp_connect(const char *connection_name)
{
    // Prepare as much as possible before entering locked stage
    rsp_message_t request;
    memset(&request, 0, sizeof(request));
    
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
    
    // make sure we truncate
    conn->connection_name = std::string(connection_name).substr(0, RSP_MAX_CONNECTION_NAME_LEN);
    // fill out struct as much as possible before locking the main map of connections
    //connection_name[RSP_MAX_CONNECTION_NAME_LEN + 1]
    strncpy(request.connection_name, connection_name, RSP_MAX_CONNECTION_NAME_LEN);
    // src_port, dst_port already 0 from memset
    // flags, 0 by default from memset
    request.flags.flags.syn = 1;
    // length -- 0 from memset
    request.length = 1;
    request.buffer[0] = 0;
    // window
    request.window = htons(g_window);
    // sequence -- 0 from memset
    // ack_sequence, nothing to ack on initial connect anyway
    // buffer[RSP_MAX_SEND_SIZE] -- no payload, empty
     
    // There is no way for anyone else to have access to this out of order lock, so there is no deadlock potential from this out of order locking
    pthread_mutex_lock(&conn->connection_state_lock);
    

    // Lock the g_OpenConnections lock
    // While holding this lock, we should atomically reserve a local connection name
    pthread_mutex_lock(&g_connectionsLock);
    // Need to make sure that we do not have conflicting name
    if (g_connections.count(connection_name) != 0)
    {
        std::cerr << "Connection already exists locally with that name." << std::endl;
        pthread_mutex_unlock(&conn->connection_state_lock);
        delete conn;
        pthread_mutex_unlock(&g_connectionsLock);
        return nullptr;
    }
    // Insert new connection
    g_connections.insert(std::pair<std::string, RspData *>(conn->connection_name, conn));
    
     // Connection inserted into list and we hold the lock for the connection, unlock the main lock
    pthread_mutex_unlock(&g_connectionsLock);
    
    // Send connection request
    if (0 != rsp_transmit(&request))
    {
        // Something wrong with the network. Give up on this connection (maybe we should give up on all connections even?)
        rsp_connect_cleanup(conn, false);
        return nullptr;
    }
    
    // Can't rsp_receive here, because that will be picked up by the RSP reader thread.
    // connection is not open until we get the response. So wait on the connection state condition
    // We already have the condition of this connection locked
    while (RSP_STATE_UNOPENED == conn->connection_state)
    {
        pthread_cond_wait(&conn->connection_state_cond, &conn->connection_state_lock);
    }
    // we have the lock back
    if (RSP_STATE_OPEN == conn->connection_state)
    {
        // if we are here, connection state is open, set up the send timer and return
        // Spin off read thread
        if (pthread_create(&(conn->timer_thread), nullptr, rsp_timer, static_cast<void *>(conn)))
        {
            rsp_connect_cleanup(conn, true);
            return nullptr;
        } 
    }
    // other option is RSP_STATE_RST
    else
    {
        rsp_connect_cleanup(conn, false);
        return nullptr;
    }
    
    pthread_mutex_unlock(&conn->connection_state_lock);
    
    
    return conn;
}

// timesSentSoFar includes this time, should already be set by caller
static void ackq_enqueue_packet(std::list<ackq_entry_t> & ackq, rsp_message_t & outgoing_packet, uint8_t timesSentSoFar)
{
    ackq_entry_t * queueItem;
    queueItem = new ackq_entry_t;
    memcpy(&(queueItem->packet), &outgoing_packet, sizeof(rsp_message_t));
    queueItem->lastSent = timestamp();
    queueItem->sendCount = timesSentSoFar;
    ackq.push_back(*queueItem);
}

int rsp_close(rsp_connection_t rsp)
{
    RspData * conn = static_cast<RspData *>(rsp);
    
    // Send fin
    rsp_message_t request;
    prepare_outgoing_packet(*conn, request);
    request.flags.flags.fin = 1;
    // length is already 0 from memset in the prepare outgoing packet function
    // ack sequence doesn't make sense, we aren't acking anything here
    // buffer has no data
    
    pthread_mutex_lock(&conn->connection_state_lock);
    conn->connection_state = RSP_STATE_WECLOSED;
    pthread_cond_broadcast(&conn->connection_state_cond);
    
    // (if sending the fin packet worked)
    if (!rsp_transmit(&request))
    {
        ackq_enqueue_packet(conn->ackq, request, 1);
        
        // Since we were able to send the fin, wait for it to be acked or timed out
        while(RSP_STATE_CLOSED != conn->connection_state && RSP_STATE_RST != conn->connection_state)
        {
            pthread_cond_wait(&conn->connection_state_cond, &conn->connection_state_lock);
        }
    }
    else
    {
        // Couldn't send. Something is quite broken getting to the RSP server
        conn->connection_state = RSP_STATE_RST;
        pthread_cond_broadcast(&conn->connection_state_cond);
    }
    
    pthread_join(conn->timer_thread, nullptr);
    
    // Ensure both queues are empty
    // As long as we aren't handling pointers, it really should be this easy for the STL one
    conn->ackq.clear();
    
    // Queue should be now closed as recv thread closes when it gets the fin in the right order
    // (or timeout times the connection out)
    // empty queue, checking each dequeue to see if it errored that the queue is empty
    rsp_message_t * messageElem = static_cast<rsp_message_t *>(Q_Dequeue_Nowait(conn->recvq));
    while (nullptr != messageElem)
    {
        delete messageElem;
        messageElem = static_cast<rsp_message_t *>(Q_Dequeue_Nowait(conn->recvq));
    }
    
    if (RSP_STATE_RST == conn->connection_state)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        delete conn;
        return -1;
    }
    else
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        delete conn;
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
    rsp_message_t outgoing_packet;
    pthread_mutex_lock(&conn->connection_state_lock);
    if (RSP_STATE_OPEN != conn->connection_state)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        return -1;
    }
    prepare_outgoing_packet(*conn, outgoing_packet);
    
    outgoing_packet.length = size;
    // LAB4 doesn't need split code but later labs will.
    memcpy(outgoing_packet.buffer, buff, std::min(size, RSP_MAX_SEND_SIZE));
    conn->current_seq += size;
    
    int transmitResult = rsp_transmit(&outgoing_packet);
    ackq_enqueue_packet(conn->ackq, outgoing_packet, 1);
    
    pthread_mutex_unlock(&(conn->connection_state_lock));
    return transmitResult;
}

int rsp_read(rsp_connection_t rsp, void *buff, int size)
{
    RspData * conn = static_cast<RspData *>(rsp);
    if (size <= 0)
    {
        return -2;
    }
    pthread_mutex_lock(&conn->connection_state_lock);
    if (RSP_STATE_OPEN != conn->connection_state)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        return -1;
    }
    // Dequeue
    rsp_message_t * incoming;
#warning need to thing through lock order of the lock in phil's queue and the locks I use -- is the order of the next two lines dangerous?
    pthread_mutex_unlock(&conn->connection_state_lock);
    incoming = static_cast<rsp_message_t *>(Q_Dequeue(conn->recvq));
    if (nullptr == incoming)
    {
        // Null with blocking call means queue is empty
        // which means closed connection. Return 0 to signal that.
        return 0;
    }
    else
    {
        // LAB4 assumes the requested size is the size of the packet,
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

