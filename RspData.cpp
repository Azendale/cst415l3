// Author: Erik B. Andersen <erik@eoni.com>
// CST415 Lab3 RSP connection tracking data
// Last modified: 2016-10-31
#include "RspData.h"
#include "rsp_if.h"

RspData::RspData(): src_port(0), dst_port(0), current_seq(0), connection_name(""), connection_state(RSP_STATE_UNOPENED), ack_highwater(-1)
{
    recvq = Q_Init();
    if (nullptr == recvq)
    {
        throw std::bad_alloc();
    }
    pthread_mutex_init(&connection_state_lock, nullptr);
    pthread_cond_init(&connection_state_cond, nullptr);
}

RspData::~RspData()
{
    if (recvq)
    {
        // Under what senarios can this fail? Unless pthread_cond_broadcast fails, I don't see how it would
        Q_Close(recvq);
        void * item;
        while (nullptr != (item = Q_Dequeue(recvq)))
        {
            delete static_cast<rsp_message_t *>(item);
        }
        Q_Destroy(recvq);
        recvq = nullptr;
    }
    pthread_mutex_destroy(&connection_state_lock);
    pthread_cond_destroy(&connection_state_cond);
}
