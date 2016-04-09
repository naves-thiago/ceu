#ifndef _CEU_THREADS_H
#define _CEU_THREADS_H

#if 0
Not using yield anymore
#include <sched.h>
#define _GNU_SOURCE
#endif

#if 0
#include <pthread.h>

#define CEU_THREADS_T               pthread_t
#define CEU_THREADS_MUTEX_T         pthread_mutex_t
#define CEU_THREADS_SELF()          pthread_self()
#define CEU_THREADS_CREATE(t,f,p)   pthread_create(t,NULL,f,p)
#define CEU_THREADS_DETACH(t)       pthread_detach(t)
#define CEU_THREADS_MUTEX_LOCK(m)   pthread_mutex_lock(m)
#define CEU_THREADS_MUTEX_UNLOCK(m) pthread_mutex_unlock(m);
#endif

#include <ChibiOS_AVR.h>

#define CEU_THREADS_T               thread_t
#define CEU_THREADS_MUTEX_T         mutex_t
#define CEU_THREADS_SELF()          chThdGetSelfX()
#define CEU_THREADS_CREATE(t,f,p)   chThdCreateFromHeap(NULL, 128, NORMALPRIO, f, p)
#define CEU_THREADS_DETACH(t)
#define CEU_THREADS_MUTEX_LOCK(m)   chMtxLock(m)
#define CEU_THREADS_MUTEX_UNLOCK(m) chMtxUnlock(m);


/*
#define CEU_THREADS_COND_T          pthread_cond_t
#define CEU_THREADS_YIELD()         assert(sched_yield()==0)
#define CEU_THREADS_YIELD()         assert(pthread_yield()==0)
#define CEU_THREADS_MUTEX_LOCK(m)   pthread_mutex_lock(m); printf("L[%d]\n",__LINE__)
#define CEU_THREADS_MUTEX_UNLOCK(m) pthread_mutex_unlock(m); printf("U[%d]\n",__LINE__)
#define CEU_THREADS_COND_WAIT(c,m)  pthread_cond_wait(c,m)
#define CEU_THREADS_COND_SIGNAL(c)  pthread_cond_signal(c)
*/

#endif
