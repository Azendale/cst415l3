/*************************************************************
 * Author:        Philip Howard
 * Filename:      prod_cons.h
 * Date Created:  5/4/2016
 * Modifications: 
 **************************************************************/
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

//#include "prod_cons.h"
#include "queue.h"

typedef struct item_s
{
    void *data;
    struct item_s *next;
} item_t;

typedef struct
{
    int closed;
    item_t *first;
    item_t *last;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} my_queue_t;

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
queue_t Q_Init()
{
    my_queue_t *queue = malloc(sizeof(my_queue_t));
    queue->first = NULL;
    queue->last = NULL;
    queue->closed = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);

    return (queue_t)queue;
}

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
int Q_Destroy(queue_t q)
{
    my_queue_t *queue = (my_queue_t *)q;
    free(queue);

    return 0;
}

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
int Q_Close(queue_t q)
{
    my_queue_t *queue = (my_queue_t *)q;

    //pthread_mutex_lock(&queue->lock);
    queue->closed = 1;
    pthread_cond_broadcast(&queue->cond);
    //pthread_mutex_unlock(&queue->lock);

    return 0;
}

/********************************************************************** 
 * Purpose: Places a new element into the queue.
 *
 * Precondition: 
 *     Queue is a valid queue that has not been marked as closed.
 *
 * Postcondition: 
 *      Queue contains one additional element
 *      Returns zero on success, non-zero on failure
 ************************************************************************/
int Q_Enqueue(queue_t q, void *buffer)
{
    my_queue_t *queue = (my_queue_t *)q;
    item_t *item = (item_t *)malloc(sizeof(item_t));
    assert(item != NULL);

    item->data = buffer;
    item->next = NULL;

    pthread_mutex_lock(&queue->lock);
    if (queue->first == NULL)
    {
        queue->first = item;
        queue->last = item;
    } else {
        queue->last->next = item;
        queue->last = item;
    }

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);

    return 0;
}

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
void *Q_Dequeue_Nowait(queue_t q)
{
    my_queue_t *queue = (my_queue_t *)q;
    char *buffer = NULL;
    item_t *item;

    pthread_mutex_lock(&queue->lock);
    if (queue->first != NULL)
    {
        item = queue->first;
        queue->first = item->next;
        if (queue->first == NULL) queue->last = NULL;

        buffer = item->data;
        free(item);
    }

    pthread_mutex_unlock(&queue->lock);

    return buffer;
}

/********************************************************************** 
 * Purpose: Removes an element from the queue
 *
 * Precondition: 
 *     Queue is a valid queue
 *
 * Postcondition: 
 *      One element was removed from the queue.
 *      Returns a pointer to the string stored in the queue.
 *
 * Note: If the queue is empty, this function will block until it is non-empty
 ************************************************************************/
void *Q_Dequeue(queue_t q)
{
    my_queue_t *queue = (my_queue_t *)q;
    char *buffer = NULL;
    item_t *item;

    pthread_mutex_lock(&queue->lock);
    while (!queue->closed && queue->first == NULL)
    {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }

    if (queue->first != NULL)
    {
        item = queue->first;
        queue->first = item->next;
        if (queue->first == NULL) queue->last = NULL;

        buffer = item->data;
        free(item);
    }

    pthread_mutex_unlock(&queue->lock);

    return buffer;
}

/********************************************************************** 
 * Purpose: Indicates whether the queue is open
 *
 * Precondition: 
 *     Queue is a valid queue
 *
 * Postcondition: 
 *      Returns zero if either the queue has not been marked as close OR
 *                             the queue is not empty
 *      Returns non-zero of BOTH: the queue has been marked as closed AND
 *                                the queue is empty
 ************************************************************************/
int Q_Is_Closed(queue_t q)
{
    my_queue_t *queue = (my_queue_t *)q;
    int result = 0;

    //pthread_mutex_lock(&queue->lock);
    if (queue->closed && queue->first == NULL) result = 1;
    //pthread_mutex_unlock(&queue->lock);

    return result;
}

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
int Q_Is_Empty(queue_t q)
{
    my_queue_t *queue = (my_queue_t *)q;
    return (queue->first == NULL);
}
