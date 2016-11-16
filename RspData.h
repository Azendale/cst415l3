// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab4 RSP connection tracking data
// Last modified: 2016-11-13
#include <string>
#include <list>
#include "queue.h"
#include "rsp_if.h"
#include <map>

using std::string;

// Connection has not been opened yet
#define RSP_STATE_UNOPENED 0
// Connection is open and useable
#define RSP_STATE_OPEN 1
// Connection was gracefully closed
#define RSP_STATE_CLOSED 2
// Something went wrong and the connection was closed without FINs
#define RSP_STATE_RST 3
// We closed the connection
#define RSP_STATE_WECLOSED 4
// They closed the connection
#define RSP_STATE_THEYCLOSED 5

typedef struct
{
    rsp_message_t packet;
    uint64_t lastSent;
    uint8_t sendCount;
} ackq_entry_t;

class RspData
{
public:
    RspData();
    ~RspData();
    
    // List for each piece of state what threads read, write it, and decide if lock is needed.
    // Thread that handles timeouts
    pthread_t timer_thread;
    // Set by rsp_connect, read by write and close, no lock needed
    // We plan to do no math on the following two variables, so by convention, they will be in network order
    uint16_t src_port;
    uint16_t dst_port;
    //uint16_t far_window;
    
    uint64_t current_seq;
    std::list<ackq_entry_t> ackq;
    string connection_name;
    queue_t recvq;
    pthread_mutex_t connection_state_lock;
    pthread_cond_t connection_state_cond;
    int connection_state;
    // Farthest we have received into the stream
    int64_t recv_highwater;
    // Farthest the remote side has reported they got
    int64_t remoteConfirm_highwater;
    // For holding out of order packets that are ahead of the next packet we expect
    std::map<uint32_t, rsp_message_t> aheadPackets;
    // For buffering outgoing packets that are waiting because of window restrictions
    queue_t sendq;
    // are we in quick start mode?
    bool quickstart;
    // counter to track runs of acks that affect window size
    uint num_acks;
};
