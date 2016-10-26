/*************************************************************
 * Author:        Philip Howard
 * Filename:      prod_cons.h
 * Date Created:  5/4/2016
 * Modifications: 
 **************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

typedef void * queue_t;

/********************************************************************** 
 * Purpose: This function initializes a queue. 
 * It returns an opaque pointer to the queue. The queue must be destroyed when
 * no longer needed by calling Q_Destroy()
 *
 * Precondition: 
 *     None
 *
 * Postcondition: 
 *      Queue has been created.
 *      Returns NULL on failure
 *
 ************************************************************************/
queue_t Q_Init();

/********************************************************************** 
 * Purpose: This function cleans up any memory occupied by the queue. 
 * It should only be called when the queue is no longer needed, and is no
 * longer being accessed by any other threads.
 *
 * Precondition: 
 *     The queue is a valid queue that has been closed and emptied.
 *     No other threads are currently accessing the queue, and none will in
 *     the future.
 *
 * Postcondition: 
 *      Queue has been destroyed; all memory has been reclaimed.
 *      Returns zero on success and non-zero on failure
 *
 ************************************************************************/
int Q_Destroy(queue_t queue);

/********************************************************************** 
 * Purpose: This markes the queue as closed. Dequeue operations are allowed
 * after a queue is marked as closed, but no further enqueue operations should
 * be performed.
 *
 * Precondition: 
 *     Queue is a valid queue.
 *
 * Postcondition: 
 *      Queue has been marked as closed.
 *      Returns zero on success, non-zero on failure
 ************************************************************************/
int Q_Close(queue_t queue);

/********************************************************************** 
 * Purpose: Places a new element into the queue.
 *
 * Precondition: 
 *     Queue is a valid queue that has not been marked as closed.
 *
 * Postcondition: 
 *      Queue contains one additional element. The new element contains
 *      the value of buffer pointer. Data is not copied, just the pointer
 *      is stored.
 *      Returns zero on success, non-zero on failure
 ************************************************************************/
int Q_Enqueue(queue_t queue, void *buffer);

/********************************************************************** 
 * Purpose: Removes an element from the queue
 *
 * Note: This call will block if the queue is empty and not closed. 
 *       See Q_Dequeue_Nowait for a non-blocking alternative.
 *
 * Precondition: 
 *     Queue is a valid queue
 *
 * Postcondition: 
 *      One element was removed from the head of the queue.
 *      Returns a pointer to the data stored in the queue.
 *      returns NULL if the queue is closed and empty.
 ************************************************************************/
void *Q_Dequeue(queue_t queue);

/********************************************************************** 
 * Purpose: Removes an element from the queue, but does not block if the 
 *          queue is empty
 *
 * Precondition: 
 *     Queue is a valid queue
 *
 * Postcondition: 
 *      If the queue was not empty, it contains one less element
 *      Returns a pointer to the string stored in the queue.
 *      Returns NULL if the queue is empty.
 ************************************************************************/
void *Q_Dequeue_Nowait(queue_t q);

/********************************************************************** 
 * Purpose: Indicates whether the queue is open
 *
 * Precondition: 
 *     Queue is a valid queue
 *
 * Postcondition: 
 *      Returns zero if either the queue has not been marked as closed OR
 *                             the queue is not empty
 *      Returns non-zero if BOTH: the queue has been marked as closed AND
 *                                the queue is empty
 ************************************************************************/
int Q_Is_Closed(queue_t queue);

/********************************************************************** 
 * Purpose: Indicates whether the queue is empty
 *
 * Precondition: 
 *     Queue is a valid queue
 *
 * Postcondition: 
 *      Returns zero if the queue contains values
 *              non-zero if the queue is empty
 *
 * NOTE: The state of the queue might change between a call to this function
 *       and a call to Q_Dequeue, so this funciton provides no guarantee as
 *       to whether a following call to Q_Dequeue will block.
 ************************************************************************/
int Q_Is_Empty(queue_t queue);

#ifdef __cplusplus
}
#endif
