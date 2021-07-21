#include "threadPool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// declarations
void* waitForTask(void* threadPool);
void emptyQueue(ThreadPool* tp);
void freeResourses(ThreadPool* tp);

ThreadPool* tpCreate(int numOfThreads) {
    ThreadPool* tp;
    if ((tp = malloc(sizeof(ThreadPool))) == NULL) {
        perror("malloc failed");
        exit(-1);
    }
    if ((tp->jobQueue = osCreateQueue()) == NULL) {
        perror("CreateQueue failed");
        free(tp);
        exit(-1);
    }
    if (pthread_mutex_init(&tp->queueMutex, NULL)) {
        perror("mutex_init failed");
        osDestroyQueue(tp->jobQueue);
        free(tp);
        exit(-1);
    }
    if (pthread_cond_init(&tp->cond, NULL)) {
        perror("cond_init failed");
        pthread_mutex_destroy(&tp->queueMutex);
        osDestroyQueue(tp->jobQueue);
        free(tp);
        exit(-1);
    }
    if ((tp->threads = (pthread_t*)malloc(numOfThreads*sizeof(pthread_t))) == NULL) {
        perror("malloc failed");
        pthread_mutex_destroy(&tp->queueMutex);
        pthread_cond_destroy(&tp->cond);
        osDestroyQueue(tp->jobQueue);
        free(tp);
        exit(-1);
    }
    if (pthread_mutex_init(&tp->wasDestroyedMutex, NULL)) {
        perror("malloc failed");
        free(tp->threads);
        pthread_mutex_destroy(&tp->queueMutex);
        pthread_cond_destroy(&tp->cond);
        osDestroyQueue(tp->jobQueue);
        free(tp);
        exit(-1);
    }
    tp->numOfThreads = numOfThreads;
    tp->wasDestroyed = 0;
    int i;
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&tp->threads[i], NULL, waitForTask, (void*)tp) != 0) {
            perror("pthread create failed");
            freeResourses(tp);
            exit(-1);
        }
    }
    return tp;
}

// take one task from the qeueue and execute it. assumes a locked queueMutex.
void doTask(ThreadPool* tp) {
    if (osIsQueueEmpty(tp->jobQueue)) {
        pthread_mutex_unlock(&tp->queueMutex);
        return;
    }
    // take a job from the queue
    funcAndArgs* job = (funcAndArgs*)osDequeue(tp->jobQueue);
    pthread_mutex_unlock(&tp->queueMutex);
    if (job) {
        // run the job
        job->func(job->args);
        // free job struct
        free(job);
    }
}

void freeResourses(ThreadPool* tp) {
    emptyQueue(tp);
    free(tp->threads);
    pthread_mutex_destroy(&tp->queueMutex);
    pthread_mutex_destroy(&tp->wasDestroyedMutex);
    pthread_cond_destroy(&tp->cond);
    osDestroyQueue(tp->jobQueue);
    free(tp);
}

// each thread runs this function. wait for a job to come and execute it.
void* waitForTask(void* threadPool) {
    ThreadPool* tp = (ThreadPool*)threadPool;
    pthread_mutex_lock(&tp->wasDestroyedMutex);
    while (!tp->wasDestroyed) {
        pthread_mutex_unlock(&tp->wasDestroyedMutex);
        pthread_mutex_lock(&tp->queueMutex);
        while (osIsQueueEmpty(tp->jobQueue) && !tp->wasDestroyed) {
            // if there are no jobs, wait for a new job to arrive.
            // thread will be signaled at arrival of a job.
            pthread_cond_wait(&tp->cond, &tp->queueMutex);
        }
        // (try to) execute a job.
        doTask(tp);
        pthread_mutex_lock(&tp->wasDestroyedMutex);
    }
    pthread_mutex_unlock(&tp->wasDestroyedMutex);
    // finish remaining tasks if needed (queue will be empty otherwise)
    while (!osIsQueueEmpty(tp->jobQueue)) {
        pthread_mutex_lock(&tp->queueMutex);
        doTask(tp);
    }
    return NULL;
}

// empty jobs queue
void emptyQueue(ThreadPool* tp) {
    pthread_mutex_lock(&tp->queueMutex);
    while (!osIsQueueEmpty(tp->jobQueue)) {
        free(osDequeue(tp->jobQueue));
    }
    pthread_mutex_unlock(&tp->queueMutex);
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    // dont allow to destroy twice
    if (threadPool->wasDestroyed)
        return;
    // printf("destroy!\n");
    pthread_mutex_lock(&threadPool->wasDestroyedMutex);
    threadPool->wasDestroyed = 1;
    pthread_mutex_unlock(&threadPool->wasDestroyedMutex);
    // empty queue if shouldWaitForTasks = 0
    if (!shouldWaitForTasks)
        emptyQueue(threadPool);
    int i;
    // signal all waiting threads
    pthread_cond_broadcast(&threadPool->cond);
    for (i = 0; i < threadPool->numOfThreads; i++) {
        pthread_join(threadPool->threads[i], NULL);
    }
    freeResourses(threadPool);
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    pthread_mutex_lock(&threadPool->wasDestroyedMutex);
    // tp was already destroyed, cant add more jobs
    if (threadPool->wasDestroyed) {
        pthread_mutex_unlock(&threadPool->wasDestroyedMutex);
        return -1;
    }
    pthread_mutex_unlock(&threadPool->wasDestroyedMutex);   
    funcAndArgs* func;
    if((func = (funcAndArgs*)malloc(sizeof(funcAndArgs))) == NULL) {
        perror("malloc failed");
        freeResourses(threadPool);
        exit(-1);
    }
    func->func = computeFunc;
    func->args = param;
    pthread_mutex_lock(&threadPool->queueMutex);
    osEnqueue(threadPool->jobQueue, func);
    // printf("job added\n");
    pthread_mutex_unlock(&threadPool->queueMutex);
    pthread_cond_signal(&threadPool->cond);
    return 0;
}

