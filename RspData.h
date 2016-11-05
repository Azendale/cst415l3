// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab3 RSP connection tracking data
// Last modified: 2016-10-31
#include <string>
#include "queue.h"

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
    // These are in HOST order, as they are going to be used in calculations.
    uint64_t far_first_sequence;
    uint64_t near_first_sequence;
    //uint16_t far_window;
    
    uint64_t current_seq;
    
    string connection_name;
    queue_t recvq;
    queue_t ackq;
    pthread_mutex_t connection_state_lock;
    int connection_state;
    int last_recv_ack_num;
};
