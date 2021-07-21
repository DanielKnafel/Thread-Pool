#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"
#include <pthread.h>

typedef struct funcAndArgs {
    void (*func) (void *);
    void* args;
} funcAndArgs;


typedef struct thread_pool
{
    pthread_mutex_t queueMutex;
    pthread_mutex_t wasDestroyedMutex;
    pthread_cond_t cond;
    int numOfThreads;
    int wasDestroyed;
    OSQueue* jobQueue;
    pthread_t* threads;
} ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
