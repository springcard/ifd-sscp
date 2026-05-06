#include "ifd-sscp.h"
#include "ifd-sscp_i.h"

BOOL CreateMutex(pthread_mutex_t *mutex)
{
    if (mutex == NULL)
        return FALSE;
    pthread_mutex_init(mutex, NULL);
    return TRUE;
}

void DestroyMutex(pthread_mutex_t *mutex)
{
    if (mutex == NULL)
        return;
    pthread_mutex_destroy(mutex);
}

BOOL CreateEvent(pthread_event_t *event)
{
    if (event == NULL)
        return FALSE;
    event->signaled = FALSE;
    pthread_mutex_init(&event->mutex, NULL);
    pthread_cond_init(&event->cond, NULL);    
    return TRUE;
}

void DestroyEvent(pthread_event_t *event)
{
    if (event == NULL)
        return;
    DestroyMutex(&event->mutex);
    pthread_cond_destroy(&event->cond);
}

BOOL SetEvent(pthread_event_t *event)
{
    if (event == NULL)
        return FALSE;
    pthread_mutex_lock(&event->mutex);
    event->signaled = TRUE;
    pthread_cond_signal(&event->cond);
    pthread_mutex_unlock(&event->mutex);
    return TRUE;
}

BOOL ClearEvent(pthread_event_t *event)
{
    if (event == NULL)
        return FALSE;
    pthread_mutex_lock(&event->mutex);
    event->signaled = FALSE;
    pthread_mutex_unlock(&event->mutex);
    return TRUE;
}

BOOL WaitEvent(pthread_event_t *event, int timeout)
{
    if (event == NULL)
        return FALSE;
    pthread_mutex_lock(&event->mutex);
    if (!event->signaled)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (timeout % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000L)
        {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }
        while (!event->signaled)
        {
            int ret = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
            if (ret == ETIMEDOUT)
            {
                pthread_mutex_unlock(&event->mutex);
                return FALSE;  /* Timeout, no event */
            }
        }
    }
    event->signaled = FALSE; /* Clear the event */
    pthread_mutex_unlock(&event->mutex);
    return TRUE; /* Got event */
}

BOOL Lock(IFDH_SSCP_INSTANCE_ST *instance)
{
    if (instance == NULL)
        return FALSE;
    pthread_mutex_lock(&instance->mutex);
    return TRUE;
}

void Unlock(IFDH_SSCP_INSTANCE_ST *instance)
{
    if (instance == NULL)
        return;
    pthread_mutex_unlock(&instance->mutex);
}