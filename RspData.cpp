#include "RspData.h"
#include "rsp_if.h"

RspData::RspData(): src_port(0), dst_port(0), far_first_sequence(0), our_first_sequence(0), far_window(0), current_seq(0), connection_name(""), connection_state(RSP_STATE_UNOPENED)
{
    recvq = Q_Init();
    if (nullptr == recvq)
    {
        throw std::bad_alloc();
    }
    pthread_mutex_init(&current_seq_lock, nullptr);
    pthread_mutex_init(&connection_state_lock, nullptr);
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
    pthread_mutex_destroy(&current_seq_lock);
    pthread_mutex_destroy(&connection_state_lock);
}
