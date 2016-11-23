// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab4 RSP implementation
// Last modified: 2016-11-14
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

#define RSP_TIMEOUT 4000
#define DEBUG true

static int g_window  = 256;
static pthread_t g_readerThread;
static bool g_readerContinue = true;
static pthread_mutex_t g_connectionsLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_packetPrintLock = PTHREAD_MUTEX_INITIALIZER;
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
static uint64_t expireDelay(uint64_t pastTimestamp)
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
static void sleepMilliseconds(uint64_t msdelay)
{
    struct timespec delay;
    delay.tv_sec =  msdelay / 1000;
    delay.tv_nsec = (msdelay%1000) * 1000000;
    nanosleep(&delay, NULL);
}

// Terminal colors, for helping with reading debug packet printouts
static const std::string red("\033[0;31m");
static const std::string green("\033[1;32m");
static const std::string yellow("\033[1;33m");
static const std::string cyan("\033[0;36m");
static const std::string magenta("\033[0;35m");
static const std::string reset("\033[0m");


static void printPacketStderr(std::string prestring, rsp_message_t & incoming_packet, std::string color)
{
    if (DEBUG)
    {
        pthread_mutex_lock(&g_packetPrintLock);
        std::cerr << color;
        std::cerr << prestring << "{connection_name = \"" << incoming_packet.connection_name << "\", src_port = " << incoming_packet.src_port << ", dst_port = " << incoming_packet.dst_port << ", flags = {syn = " << +incoming_packet.flags.flags.syn << ", ack = " << +incoming_packet.flags.flags.ack << ", fin = "<< +incoming_packet.flags.flags.fin << ", rst = "<< +incoming_packet.flags.flags.rst << ", err = " << +incoming_packet.flags.flags.err << ", nod = " << +incoming_packet.flags.flags.nod << ", nro = "<< +incoming_packet.flags.flags.nro << ", reserved = "<< +incoming_packet.flags.flags.reserved << "}}, length = "<< +incoming_packet.length << ", window = " << ntohs(incoming_packet.window) << ", sequence = " << ntohl(incoming_packet.sequence) << ", ack_sequence = "<< ntohl(incoming_packet.ack_sequence) << "}";
        std::cerr <<  reset << std::endl;
        pthread_mutex_unlock(&g_packetPrintLock);
    }
}

static int rsp_transmit_wrap(rsp_message_t * packet)
{
    // Comment if you don't want a packet capture on stderr
    printPacketStderr("Transmit: ", *packet, green);
    return rsp_transmit(packet);
}

static int rsp_receive_wrap(rsp_message_t * packet)
{
    int result = rsp_receive(packet);
    // Comment if you don't want a packet capture on stderr
    printPacketStderr("Recieve:  ", *packet, cyan);
    return result;
}

// Figure out how long we need to wait for the packet in the front of the ackq to timeout, and 
// store the amount of time to wait in delay. Store the end of the byte range the packet completes
// in sequenceTotal
static void getNextAckqPacketDelay(RspData * conn, uint64_t & delay)
{
    if (conn->ackq.empty())
    {
        delay = RSP_TIMEOUT;
    }
    else
    {
        ackq_entry_t & waitingPacket = conn->ackq.front();
        delay = expireDelay(waitingPacket.lastSent);
    }
}

static void prepare_outgoing_packet(RspData & conn, rsp_message_t & packet)
{
    memset(&packet, 0, sizeof(packet));
    strncpy(packet.connection_name, conn.connection_name.c_str(), RSP_MAX_CONNECTION_NAME_LEN);
    packet.src_port = conn.src_port;
    packet.dst_port = conn.dst_port;
    // This is ignored
    packet.window = htons(conn.window);
    // Not totally sure this shouldn't be set by the calling function. We'll see
    packet.sequence = htonl(conn.current_seq);
}

// retransmit lost packet -- retransmits the packet on the top of the ackq
// returns false if this is the third time or we weren't able to send
// precondition: there must actually be a packet in the head of the ackq
static bool retransmitHeadPacket(RspData * conn)
{
    ackq_entry_t & lostPacket = conn->ackq.front();
    if (lostPacket.sendCount < 3)
    {
        // Set next packet timeout farther away in case only one packet was actually dropped and waiting for an ack would get a bunch of packets removed from the ack waiting list
        for (auto packetTimeoutEntry = conn->ackq.begin(); packetTimeoutEntry != conn->ackq.end(); ++packetTimeoutEntry)
        {
            packetTimeoutEntry->lastSent = timestamp();
        }       
        
        // Now actually resend the packet at the head of the line
        //lostPacket.lastSent = timestamp();
        lostPacket.sendCount += 1;
        return ! rsp_transmit_wrap(&lostPacket.packet);
    }
    else
    {
        return false;
    }
}

// One timer thread per connection
static void * rsp_timer(void * args)
{
    RspData * conn = static_cast<RspData *>(args);
    uint64_t expireDelay;

    pthread_mutex_lock(&conn->connection_state_lock);
    getNextAckqPacketDelay(conn, expireDelay);
    
    while (RSP_STATE_OPEN == conn->connection_state && ! conn->ourCloseAcked)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        sleepMilliseconds(expireDelay);
        
        pthread_mutex_lock(&conn->connection_state_lock);
        // Were we waiting on packets? (if not, we were waiting for packets to wait on!)
        if (!conn->ackq.empty())
        {
            // If so, did the front one timeout while we were alseep?
            if ( (!conn->ackq.empty()) && conn->ackq.front().lastSent + RSP_TIMEOUT < timestamp())
            {
                // Dropped a packet, resize window
                conn->window /= 2;
                if (conn->window < 1)
                {
                    conn->window = 1;
                }
                conn->quickstart = false;
                if (DEBUG)
                {
                    std::cerr << "Dropped packet, window is now adjusted to " << +conn->window << ", packet ts " << conn->ackq.front().lastSent << ", now " << timestamp() << "." << std::endl;
                }
                
                printPacketStderr("tmout pkt:", conn->ackq.front().packet, magenta);
                
                // packet was not acked, it is the first in the queue
                // Returns false if this is more than the third time or we fail to transmit
                if (!retransmitHeadPacket(conn))
                {
                    if (DEBUG)
                    {
                        std::cerr << "Killing connection " << conn->connection_name << " after packet seq " << htonl(conn->ackq.front().packet.sequence) << " and len " << +conn->ackq.front().packet.length << " timed out 3 times." << std::endl;
                    }
                    conn->connection_state = RSP_STATE_RST;
                    rsp_message_t outgoing_packet;
                    prepare_outgoing_packet(*conn, outgoing_packet);
                    outgoing_packet.flags.flags.rst = 1;
                    rsp_transmit_wrap(&outgoing_packet);
                    pthread_cond_broadcast(&conn->connection_state_cond);
                    pthread_mutex_unlock(&conn->connection_state_lock);
                    return nullptr;
                }
            }
        }
        
        getNextAckqPacketDelay(conn, expireDelay);
    }
    pthread_mutex_unlock(&conn->connection_state_lock);
    
    return nullptr;
}

static void sendAcket(RspData & conn, uint8_t length)
{
    if (0 < length)
    {
        rsp_message_t ackPacket;
        prepare_outgoing_packet(conn, ackPacket);
        ackPacket.ack_sequence = htonl(conn.recv_highwater);
        ackPacket.flags.flags.ack = 1;
        // We only process their fin in order. So if this state flag is set, then we have hit their FIN packet in the stream, and a cumulative ack would include acking the fin, so set the fin flag for this (now) FIN+ACK packet
        if (conn.theirCloseRecieved)
        {
            ackPacket.flags.flags.fin = 1;
        }
        // send ack
        rsp_transmit_wrap(&ackPacket);
    }
}

// Assumes caller has conn locked
// Sends packets from the waiting sendq
// NOT for resending timed out packets (then you would have duplicates in ackq)
void check_send(RspData & conn)
{
    int send_permitted_count = conn.window - conn.ackq.size();
    rsp_message_t * queuePacket = nullptr;
    while (send_permitted_count > 0 && (queuePacket = static_cast<rsp_message_t *>(Q_Dequeue_Nowait(conn.sendq))))
    {
        ackq_entry_t ackEntry;
        --send_permitted_count;
        ackEntry.lastSent = timestamp();
        ackEntry.sendCount = 1;
        memcpy(&(ackEntry.packet), queuePacket, sizeof(rsp_message_t));
        conn.ackq.push_back(ackEntry);
        rsp_transmit_wrap(queuePacket);
        delete queuePacket;
    }
}

// Assumes that locks g_connectionsLock and thisConn->connection_state_lock are locked
// Caller's reponsibility to ensure that this is only called on packets where 
// thisConn->recv_highwater == ntohl(incoming_packet.sequence)
// IE only call it with the next packet
// caller is expected to send acks for runs of packets, but this function will update
// recv_highwater
static void process_acked_packet(RspData * thisConn, rsp_message_t & incoming_packet)
{
    // Update sequence highwater
    thisConn->recv_highwater = ntohl(incoming_packet.sequence) + incoming_packet.length;
    
    // Is packet FIN
    if (incoming_packet.flags.flags.fin)
    {
        // Fin/acks not handled in this function, already handled by ack packet handling machinery
        if (!incoming_packet.flags.flags.ack)
        {
            // // Done already at the end of this 'if' block
            // pthread_cond_broadcast(&thisConn->connection_state_cond);
            thisConn->theirCloseRecieved = true;
            Q_Close(thisConn->recvq);
            // Next 7 lines shouldn't be needed, should be covered by sendAcket function now that theirCloseRecieved is set
            //rsp_message_t lastFin;
            //prepare_outgoing_packet(*thisConn, lastFin);
            //lastFin.flags.flags.fin = 1;
            //lastFin.flags.flags.ack = 1;
            //// Expecting that this incoming packet already updated the recv_highwater
            //lastFin.ack_sequence = htonl(thisConn->recv_highwater);
            //rsp_transmit_wrap(&lastFin);
            }
        
        // We have the last packet from them, and the last ack from them
        if (thisConn->theirCloseRecieved && thisConn->ourCloseAcked)
        {
            // // Allow close to erase us from the main map
            // g_connections.erase(it);
            thisConn->connection_state = RSP_STATE_CLOSED;
        }
        pthread_cond_broadcast(&thisConn->connection_state_cond);
        return;

    }
    // don't enqueue data byte from fin packet, so "else if" instead of only if
    // if we have any payload
    else if (0 < incoming_packet.length)
    {
        rsp_message_t * queuedpacket = new rsp_message_t;
        memcpy(queuedpacket, &incoming_packet, sizeof(rsp_message_t));
        Q_Enqueue(thisConn->recvq, queuedpacket);
    }

}


// Assumes that locks g_connectionsLock and thisConn->connection_state_lock are locked
static void process_incoming_packet(RspData * thisConn, rsp_message_t & incoming_packet)
{
    if (incoming_packet.flags.flags.err)
    {
        // (apparently err does not mean the connection is dead?) 
        return;
    }
    // Is packet RST
    else if (incoming_packet.flags.flags.rst)
    {
        thisConn->connection_state = RSP_STATE_RST;
        pthread_cond_broadcast(&thisConn->connection_state_cond);
        return;
    }
    // Is packet SYN + ACK
    else if (incoming_packet.flags.flags.syn && incoming_packet.flags.flags.ack)
    {
        // SYNACK
        // Parse src and dest port, save them
        thisConn->src_port = incoming_packet.src_port;
        thisConn->dst_port = incoming_packet.dst_port;
        
        // Null terminate the name of the string, just in case it is not
        incoming_packet.connection_name[RSP_MAX_CONNECTION_NAME_LEN] = '\0';
        thisConn->connection_name = string(incoming_packet.connection_name);
        
        thisConn->recv_highwater = ntohl(incoming_packet.sequence) + incoming_packet.length;
        
        thisConn->connection_state = RSP_STATE_OPEN;
        pthread_cond_broadcast(&thisConn->connection_state_cond);
        return;
    }
    
    // Is packet an ACK
    if (incoming_packet.flags.flags.ack)
    {
        uint32_t receivedThru = ntohl(incoming_packet.ack_sequence);
        
        // take stuff out of the timeout queue when it is acked
        while (! thisConn->ackq.empty() && ntohl(thisConn->ackq.front().packet.sequence) + thisConn->ackq.front().packet.length <= receivedThru)
        {
            printPacketStderr("rm_ackq:  ", thisConn->ackq.front().packet, red);
            if (thisConn->ackq.front().packet.flags.flags.fin)
            {
                // Setting this kills the timer next time it wakes
                thisConn->ourCloseAcked = true;
                pthread_cond_broadcast(&thisConn->connection_state_cond);
                
                // We have the last packet from them, and the last ack from them
                if (thisConn->theirCloseRecieved && thisConn->ourCloseAcked && RSP_STATE_RST != thisConn->connection_state)
                {
                    // // Allow close to erase us from the main map
                    // g_connections.erase(it);
                    thisConn->connection_state = RSP_STATE_CLOSED;
                    pthread_cond_broadcast(&thisConn->connection_state_cond);
                }
            }
            
            // Something came out the the ackq, adjust window
            if (thisConn->quickstart)
            {
                ++thisConn->window;
                thisConn->ackrun = 0;
                if (DEBUG)
                {
                    std::cerr << "Progress made after receiving ack, 'slow'start mode, window now " <<  +thisConn->window << "." << std::endl;
                }
            }
            else
            {
                ++thisConn->ackrun;
                if (thisConn->ackrun >= thisConn->window)
                {
                   ++thisConn->window;
                   thisConn->ackrun = 0;
                    if (DEBUG)
                    {
                        std::cerr << "Progress made after receiving ack, linear mode, window now " <<  +thisConn->window << " and ackrun reset." << std::endl;
                    }
                }
                else
                {
                    if (DEBUG)
                    {
                        std::cerr << "Progress made after receiving ack, linear mode, window now " << +thisConn->window << "." << std::endl;
                    }
                }
            }
            thisConn->ackq.pop_front();
        }
        
        // Make sure acks can only move forward
        if (thisConn->remoteConfirm_highwater < receivedThru)
        {
            thisConn->remoteConfirm_highwater = receivedThru;
            check_send(*thisConn);
        }
    }
    
    // if we have any payload (and therefore the other end will see an ack as acking this packet)
    if (0 < incoming_packet.length)
    {
        uint32_t incomingSeq = ntohl(incoming_packet.sequence);
        if (incomingSeq >= thisConn->recv_highwater)
        {
            // Packet is not a repeat, so it will either be enqueued to the data queue or put in the out of order map
            if (incomingSeq == thisConn->recv_highwater)
            {
                // A packet for "now" in the stream
                process_acked_packet(thisConn, incoming_packet);
                std::map<uint32_t, rsp_message_t>::iterator nextPkt;
                // While there is a packet in the ahead map that is next in line ...
                while (!thisConn->aheadPackets.empty() && (nextPkt = thisConn->aheadPackets.find(thisConn->recv_highwater)) != thisConn->aheadPackets.end())
                {
                    // ... process it also
                    if (DEBUG)
                    {
                        std::cerr << "Pulling packet seq " << +thisConn->recv_highwater << " from the ahead queue." << std::endl;
                    }
                    process_acked_packet(thisConn, nextPkt->second);
                    thisConn->aheadPackets.erase(nextPkt);
                }
            }
            else
            {
                // A packet from the future -- put it in the map
                // remember that key is sequence IN HOST ORDER
                thisConn->aheadPackets[ntohl(incoming_packet.sequence)] = incoming_packet;
            }
        }
        // This will either ack what we have already, or ack a run of packets, including FINACKing if we have seen their fin
        sendAcket(*thisConn, incoming_packet.length);
    }
}

static void * rsp_reader(void * args)
{
    rsp_message_t incoming_packet;
    bool readCont = true;
    
    while (readCont)
    {
        // Block for a read
        memset(&incoming_packet, 0, sizeof(incoming_packet));
        rsp_receive_wrap(&incoming_packet);
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
            if (DEBUG)
            {
                std::cerr << "Got a packet for connection name " << connName << " but there is no active connection by that name." << std::endl;
            }
        
            pthread_mutex_unlock(&g_connectionsLock);
            continue;
        }
        // Shouldn't rely on the interator for our reference, because we may remove from the container.
        // Note that the memory is still valid because the client holds a pointer to it and it has not been deleted
        RspData * thisConn = it->second;
        
        // Handle packet
        pthread_mutex_lock(&thisConn->connection_state_lock);
        
        // Assumes that locks g_connectionsLock and thisConn->connection_state_lock are locked
        process_incoming_packet(thisConn, incoming_packet);
        
        pthread_mutex_unlock(&thisConn->connection_state_lock);
        pthread_mutex_unlock(&g_connectionsLock);
        
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
    rsp_transmit_wrap(&reflection);
    
    pthread_join(g_readerThread, nullptr);
    pthread_mutex_destroy(&g_connectionsLock);
    pthread_mutex_destroy(&g_packetPrintLock);
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
        rsp_transmit_wrap(&request);
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
    conn->window = g_window;
    

    // Lock the g_OpenConnections lock
    // While holding this lock, we should atomically reserve a local connection name
    pthread_mutex_lock(&g_connectionsLock);
    // Need to make sure that we do not have conflicting name
    if (g_connections.count(connection_name) != 0)
    {
        if (DEBUG)
        {
            std::cerr << "Connection already exists locally with that name." << std::endl;
        }
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
    if (0 != rsp_transmit_wrap(&request))
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
    ackq_entry_t queueItem;
    memcpy(&(queueItem.packet), &outgoing_packet, sizeof(rsp_message_t));
    queueItem.lastSent = timestamp();
    queueItem.sendCount = timesSentSoFar;
    ackq.push_back(queueItem);
}

int rsp_close(rsp_connection_t rsp)
{
    RspData * conn = static_cast<RspData *>(rsp);
    
    // Send fin
    rsp_message_t request;
    pthread_mutex_lock(&conn->connection_state_lock);
    // Always send a fin, as long as we haven't before
    if (! conn->ourCloseSent && RSP_STATE_RST != conn->connection_state)
    {
        Q_Close(conn->sendq);
        prepare_outgoing_packet(*conn, request);
        request.flags.flags.fin = 1;
        // fin can have a single byte that gets thrown out according to Phil
        request.length = 1;
        request.sequence = htonl(conn->current_seq);
        // buffer has no data
        
        pthread_cond_broadcast(&conn->connection_state_cond);
        
        ackq_enqueue_packet(conn->ackq, request, 1);
        // (if sending the fin packet failed)
        if (rsp_transmit_wrap(&request))
        {
            // Couldn't send. Something is quite broken getting to the RSP server
            pthread_cond_broadcast(&conn->connection_state_cond);
            conn->connection_state = RSP_STATE_RST;
        }
        conn->ourCloseSent = true;
    }
    
    // Since we were able to send the fin, wait for it to be acked or timed out
    while(RSP_STATE_RST != conn->connection_state && !(conn->ourCloseAcked && conn->theirCloseRecieved))
    {
        pthread_cond_wait(&conn->connection_state_cond, &conn->connection_state_lock);
    }
    
    // Unlock so the timer can see it should stop
    pthread_mutex_unlock(&conn->connection_state_lock);
    
    pthread_join(conn->timer_thread, nullptr);
    
    // Note, I think this is already handled in the read thread
    pthread_mutex_lock(&g_connectionsLock);
    pthread_mutex_lock(&conn->connection_state_lock);
    // Remove from incoming list to make it so it will not be referenced by reader after the following cleanup.
    auto it = g_connections.find(conn->connection_name);
    if (g_connections.cend() != it)
    {
        g_connections.erase(it);
    }
    pthread_mutex_unlock(&g_connectionsLock);
    // Not worried about checking the state here, because by convention, it is not possible to go from 
    // RSP_STATE_CLOSED to anything else, and it is also not possible to go from RSP_STATE_RST to anything 
    // else. If functions aren't following that, then I guess I also can't trust them to use locks...
    
    // Ensure both queues are empty
    // easy as long as it is the STL with no pointers
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
    rsp_message_t * outgoing_packet;
    pthread_mutex_lock(&conn->connection_state_lock);
    if (RSP_STATE_OPEN != conn->connection_state)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        return -1;
    }
    
    outgoing_packet = new rsp_message_t;
    
    if (nullptr == outgoing_packet)
    {
        pthread_mutex_unlock(&conn->connection_state_lock);
        return -2;
    }
    prepare_outgoing_packet(*conn, *outgoing_packet);
    
    outgoing_packet->length = size;
    // LAB4 doesn't need split code but later labs will.
    memcpy(outgoing_packet->buffer, buff, std::min(size, RSP_MAX_SEND_SIZE));
    conn->current_seq += size;
    
    Q_Enqueue(conn->sendq, outgoing_packet);
    check_send(*conn);
    
    pthread_mutex_unlock(&(conn->connection_state_lock));
    return 0;
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

