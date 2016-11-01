#include <string>
#include "queue.h"

using std::string;

#define RSP_STATE_UNOPENED 0
#define RSP_STATE_OPEN 1
#define RSP_STATE_CLOSED 2
#define RSP_STATE_RST 3


class RspData
{
public:
    RspData();
    ~RspData();
    
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
    
    pthread_mutex_t current_seq_lock;
    uint64_t current_seq;
    
    string connection_name;
    queue_t recvq;
    pthread_mutex_t connection_state_lock;
    int connection_state;
};
