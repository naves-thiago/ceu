#include "ceu_os.h"

#ifdef CEU_ISRS
    #ifndef ceu_out_isr_on
        #error "Missing definition for macro \"ceu_out_isr_on\"."
    #endif
    #ifndef ceu_out_isr_off
        #error "Missing definition for macro \"ceu_out_isr_off\"."
    #endif
    #ifndef ceu_out_isr_attach
        #error "Missing definition for macro \"ceu_out_isr_attach\"."
    #endif
    #ifndef ceu_out_isr_detach
        #error "Missing definition for macro \"ceu_out_isr_detach\"."
    #endif
#endif

#include <string.h>

#ifdef CEU_DEBUG
#include <stdio.h>      /* printf */
#endif

#if defined(CEU_DEBUG) || defined(CEU_NEWS) || defined(CEU_THREADS) || defined(CEU_OS_KERNEL)
void *realloc(void *ptr, size_t size);
#endif

#ifdef CEU_NEWS_POOL
#include "ceu_pool.h"
#endif

/*
 * pthread_t thread;
 * pthread_mutex_t mutex;
 * pthread_cond_t  cond;
 * pthread_self();
        Uint32 SDL_ThreadID(void);
 * pthread_create(&thread, NULL, f, &p);
        SDL_Thread *SDL_CreateThread(int (*fn)(void *), void *data);
 * pthread_mutex_lock(&mutex);
 * pthread_mutex_unlock(&mutex);
 * pthread_cond_wait(&cond, &mutex);
 * pthread_cond_signal(&cond);
*/

/**********************************************************************
 * "APPS" running on the OS do not need any of the below.
 **********************************************************************/

#ifndef CEU_OS_APP

#ifdef CEU_NEWS
#ifdef CEU_RUNTESTS
#define CEU_MAX_DYNS 100
static int _ceu_dyns_ = 0;  /* check if total of alloc/free match */
#endif
#endif

#if defined(CEU_NEWS) || defined(CEU_THREADS) || defined(CEU_OS_KERNEL) || defined(CEU_VECTOR_MALLOC)
void* ceu_sys_realloc (void* ptr, size_t size) {
#ifdef CEU_NEWS
#ifdef CEU_RUNTESTS
    if (size == 0) {
        if (ptr != NULL) {
            _ceu_dyns_--;
        }
    } else {
        if (_ceu_dyns_ >= CEU_MAX_DYNS) {
            return NULL;
        }
        _ceu_dyns_++;           /* assumes no malloc fails */
    }
#endif
#endif
    return realloc(ptr, size);
}
#endif

#ifdef CEU_VECTOR
#include "ceu_vector.h"
byte* ceu_vector_geti_ex (tceu_vector* vector, int idx, char* file, int line) {
    byte* ret = ceu_vector_geti(vector, idx);
    ceu_out_assert_msg_ex(ret!=NULL, "access out of bounds", file, line);
    return ret;
}
#endif

int CEU_REQS = 0;
int ceu_sys_req (void) {
    CEU_REQS++;
    return CEU_REQS;
}

void ceu_sys_go_ex (tceu_app* app, tceu_evt* evt,
                    tceu_stk* stk,
                    tceu_org* org, tceu_ntrl trl0, tceu_ntrl trlF);

/**********************************************************************/

void ceu_sys_org_init (tceu_org* org, int n, int lbl,
                       int cls, int isDyn,
                       tceu_org* parent_org, tceu_ntrl parent_trl)
{
    /* { evt=0, seqno=0, lbl=0 } for all trails */
    memset(&org->trls, 0, n*sizeof(tceu_trl));
    org->trls[0].lbl = lbl;

#if defined(CEU_ORGS) || defined(CEU_OS_KERNEL)
    org->n  = n;
#endif

#ifdef CEU_ORGS

#ifdef CEU_IFCS
    org->cls = cls;
#endif

#if defined(CEU_ORGS_NEWS) || defined(CEU_ORGS_AWAIT)
    org->isAlive = 1;
#endif

#ifdef CEU_ORGS_NEWS
    org->isDyn = isDyn;
#endif

    org->parent_org = parent_org;
    org->parent_trl = parent_trl;
    org->nxt = NULL;
    if (parent_org != NULL) {
        tceu_trl* trl = &parent_org->trls[parent_trl];
        if (trl == NULL) {
            org->prv = NULL; /* main class */
        } else {
            /* re-link */
            if (trl->org == NULL) {
                trl->org = org;
            } else {
                tceu_org* last = trl->org->prv;
                last->nxt = org;
                org->prv = last;
            }
            trl->org->prv = org;
        }
    }

#ifdef CEU_ORGS_AWAIT
    org->ret = 0;   /* TODO: still required? */
#endif

#endif  /* CEU_ORGS */
}

#ifdef CEU_ORGS

static void ceu_sys_org_free (tceu_app* app, tceu_org* org)
{
    /* TODO: try to not depend on this and remove this field */
    if (org->isAlive) {
        org->isAlive = 0;
    } else {
        return;
    }

    /* re-link PRV <-> NXT */
    /* relink also static orgs for efficiency */
    tceu_trl* trl = &org->parent_org->trls[org->parent_trl];
    if (trl->org == org) {
        trl->org = org->nxt;        /* subst 1st org */
    } else {
        org->prv->nxt = org->nxt;
    }
    if (org->nxt == NULL) {
        if (trl->org != NULL) {
            trl->org->prv = org->prv;   /* subst lst org */
        }
    } else {
        org->nxt->prv = org->prv;
    }

#ifdef CEU_ORGS_NEWS
    /* free */
    if (org->isDyn) {
#if    defined(CEU_ORGS_NEWS_POOL) && !defined(CEU_ORGS_NEWS_MALLOC)
        ceu_pool_free(&org->pool->pool, (byte*)org);
#elif  defined(CEU_ORGS_NEWS_POOL) &&  defined(CEU_ORGS_NEWS_MALLOC)
        if (org->pool->pool.queue == NULL) {
            org->nxt = app->tofree;
            app->tofree = org;
        } else {
            ceu_pool_free(&org->pool->pool, (byte*)org);
        }
#elif !defined(CEU_ORGS_NEWS_POOL) &&  defined(CEU_ORGS_NEWS_MALLOC)
        org->nxt = app->tofree;
        app->tofree = org;
#endif
    }
#endif
}

/*
 * Checks if "me" is cleared due to a clear in "clr_org".
 * ;
 */
static int ceu_org_is_cleared (tceu_org* me, tceu_org* clr_org,
                               tceu_ntrl clr_t1, tceu_ntrl clr_t2)
{
    if (me == clr_org) {
        return (clr_t1==0 && clr_t2==me->n-1);
    }

    tceu_org* cur_org;
    for (cur_org=me; cur_org!=NULL; cur_org=cur_org->parent_org) {
        if (cur_org->parent_org == clr_org) {
            if (cur_org->parent_trl>=clr_t1 && cur_org->parent_trl<=clr_t2) {
                return 1;
            }
        }
    }
    return 0;
}

#endif  /* CEU_ORGS */

/**********************************************************************/

#ifdef CEU_STACK_CLEAR
void ceu_sys_stack_dump (tceu_stk* stk) {
    printf(">>> STACK DUMP:\n");
    for (; stk!=NULL; stk=stk->down) {
        printf("\t[%p] down=%p org=%p trls=[%d,%d]\n",
            stk, stk->down, stk->org, stk->trl1, stk->trl2);
    }
}

/*
 * Trails [t1,t2] of "org" are dyeing.
 * Traverse the stack to see if a pending call is enclosed by this range.
 * If so, the whole stack has to unwind and continue from what we pass in 
 * lbl_or_org.
 */
void ceu_sys_stack_clear (tceu_stk* stk, tceu_org* org,
                          tceu_ntrl t1, tceu_ntrl t2) {
    for (; stk->down!=NULL; stk=stk->down) {
        if (!stk->is_alive) {
            continue;
        }
#ifdef CEU_ORGS
        if (stk->org != org) {
            if (ceu_org_is_cleared(stk->org, org, t1, t2)) {
                stk->is_alive = 0;
            }
        }
        else
#endif
        {
            if (t1<=stk->trl1 && stk->trl2<=t2) {
                stk->is_alive = 0;
            }
        }
    }
}
#endif

/**********************************************************************/

#ifdef CEU_WCLOCKS

/* TODO: wclk_min_cmp to be global among all apps */

int ceu_sys_wclock (tceu_app* app, s32 dt, s32* set, s32* get)
{
    s32 t;          /* expiring time of track to calculate */
    int ret = 0;    /* if track expired (only for "get") */

    /* SET */
    if (set != NULL) {
        t = dt - app->wclk_late;
        *set = t;

    /* CHECK */
    } else {
        t = *get;
        if ((t > app->wclk_min_cmp) || (t > dt)) {
            *get -= dt;    /* don't expire yet */
            t = *get;
        } else {
            ret = 1;    /* single "true" return */
        }
    }

    /* didn't awake, but can be the smallest wclk */
    if ( (!ret) && (app->wclk_min_set > t) ) {
        app->wclk_min_set = t;
#ifdef ceu_out_wclock_set
        ceu_out_wclock_set(t);
#endif
    }

    return ret;
}

#ifdef CEU_TIMEMACHINE
/* TODO: unify with above */
int ceu_sys_wclock_ (tceu_app* app, s32 dt, s32* set, s32* get)
{
    s32 t;          /* expiring time of track to calculate */
    int ret = 0;    /* if track expired (only for "get") */

    /* SET */
    if (set != NULL) {
        t = dt - app->wclk_late;
        *set = t;

    /* CHECK */
    } else {
        t = *get;
        if ((t > app->wclk_min_cmp_) || (t > dt)) {
            *get -= dt;    /* don't expire yet */
            t = *get;
        } else {
            ret = 1;    /* single "true" return */
        }
    }

    /* didn't awake, but can be the smallest wclk */
    if ( (!ret) && (app->wclk_min_set_ > t) ) {
        app->wclk_min_set_ = t;
#ifdef ceu_out_wclock_set
        ceu_out_wclock_set(t);
#endif
    }

    return ret;
}
#endif

#endif

/**********************************************************************/

#ifdef CEU_LUA
int ceu_lua_atpanic_f (lua_State* lua) {
#ifdef CEU_DEBUG
    char msg[255] = "LUA_ATPANIC: ";
    strncat(msg, lua_tostring(lua,-1), 100);
    strncat(msg, "\n", 1);
    ceu_out_assert_msg(0, msg);
/*
*/
#else
    ceu_out_assert_msg(0, "bug found");
#endif
    return 0;
}
#endif

/**********************************************************************/

#ifdef CEU_PSES
#ifdef CEU_OS_KERNEL
#error Not implemented!
#endif
void ceu_pause (tceu_trl* trl, tceu_trl* trlF, int psed) {
    do {
        if (psed) {
            if (trl->evt == CEU_IN__ORG) {
                trl->evt = CEU_IN__ORG_PSED;
            }
        } else {
            if (trl->evt == CEU_IN__ORG_PSED) {
                trl->evt = CEU_IN__ORG;
            }
        }
    } while (++trl <= trlF);

#ifdef ceu_out_wclock_set
    if (!psed) {
        ceu_out_wclock_set(0);  /* TODO: recalculate MIN clock */
                                /*       between trl => trlF   */
    }
#endif
#ifdef CEU_TIMEMACHINE
#ifdef ceu_out_wclock_set_
    if (!psed) {
        ceu_out_wclock_set_(0);  /* TODO: recalculate MIN clock */
                                 /*       between trl => trlF   */
    }
#endif
#endif
}
#endif

/**********************************************************************/

#ifdef CEU_OS_KERNEL
u8 CEU_GC = 0;  /* execute __ceu_os_gc() when "true" */
#endif

#ifdef CEU_DEBUG_TRAILS
static int spc = -1;
#define SPC(n) { int i; for(i=0; i<(spc+n)*4; i++) printf(" "); };

void ceu_sys_go_ex_dbg (tceu_app* app, tceu_evt* evt,
                        tceu_stk* stk,
                        tceu_org* org, tceu_ntrl trl0, tceu_ntrl trlF);
void ceu_sys_go_ex (tceu_app* app, tceu_evt* evt,
                    tceu_stk* stk,
                    tceu_org* org, tceu_ntrl trl0, tceu_ntrl trlF) {
    spc++;
    SPC(0); printf(">>> GO-EX\n");
    SPC(0); printf("evt: %d\n", evt->id);
    #ifdef CEU_ORGS
    SPC(0); printf("org: %p\n", org);
    SPC(2); printf("[%p]=>[%p]\n", &org->trls[0],
                                   &org->trls[org->n]);
    #endif

    ceu_sys_go_ex_dbg(app,evt,stk,org,trl0,trlF);

    SPC(0); printf("<<< GO-EX\n");
    spc--;
}
#endif

#ifdef CEU_DEBUG_TRAILS
void ceu_sys_go_ex_dbg (tceu_app* app, tceu_evt* evt,
                        tceu_stk* stk,
                        tceu_org* org, tceu_ntrl trl0, tceu_ntrl trlF)
#else
void ceu_sys_go_ex (tceu_app* app, tceu_evt* evt,
                    tceu_stk* stk,
                    tceu_org* org, tceu_ntrl trl0, tceu_ntrl trlF)
    /* TODO: now all arguments are required in all configurations */
#endif
{
    tceu_ntrl trlI;
    tceu_trl* trl;
    for (trlI=trl0, trl=&org->trls[trlI];
#ifdef CEU_STACK_CLEAR
         stk->is_alive &&
#endif
            trlI<trlF;
         trlI++, trl++)
    {
#ifdef CEU_DEBUG_TRAILS
SPC(1); printf("trl: %p\n", trl);
/*SPC(2); printf("seqno: %d\n", trl->seqno);*/
SPC(2); printf("evt: %d\n", trl->evt);
SPC(2); printf("lbl: %d\n", trl->lbl);
#endif

        /* continue traversing current org */

        /* jump into linked orgs */
#ifdef CEU_ORGS
        if ( (trl->evt == CEU_IN__ORG)
#ifdef CEU_PSES
          || (trl->evt==CEU_IN__ORG_PSED && evt->id==CEU_IN__CLEAR)
#endif
           )
        {
            tceu_org* cur = trl->org;

            if (evt->id == CEU_IN__CLEAR) {
                trl->evt = CEU_IN__NONE;    /* TODO: dup w/ below */
            }

            /* traverse all children */
            if (cur != NULL) {
#ifdef CEU_STACK_CLEAR
                tceu_stk stk_ = { stk, org, cur->parent_trl, cur->parent_trl, 1 };
#endif
                while (cur != NULL) {
                    tceu_org* nxt = cur->nxt;   /* save before possible free/relink */
#ifdef CEU_STACK_CLEAR
                    ceu_sys_go_ex(app, evt, &stk_, cur, 0, cur->n);
                    if (!stk->is_alive) {
                        return; /* whole outer traversal aborted */
                    }
#if 0
if (!stk_.is_alive) {
printf("aborted\n");
    break; /* all children traversal aborted */
}
#endif
#else
                    ceu_sys_go_ex(app, evt, NULL, cur, 0, cur->n);
#endif
                    cur = nxt;
                }
            }
            continue;   /* next trail after handling children */
        }
#endif /* CEU_ORGS */

        /* EXECUTE THIS TRAIL */
#if 0
printf("%d==%d && %d!=%d && %d>=%d\n",
        trl->evt, evt->id,
        trl->seqno, app->seqno,
        evt->id, CEU_IN_lower
);
if (evt->param != NULL) {
    printf("trl->org_or_adt=%p // param=%p\n", trl->org_or_adt,
                 ((tceu_kill*)evt->param)->org_or_adt);
}
#endif

        if (
#ifdef CEU_CLEAR
            /* if IN__CLEAR and "finalize" clause */
            (evt->id==CEU_IN__CLEAR && trl->evt==CEU_IN__CLEAR)
        ||
#endif
#ifdef CEU_ORGS_AWAIT
            /* if */
            (evt->id==CEU_IN__ok_killed && trl->evt==CEU_IN__ok_killed &&
#ifdef CEU_ADTS_AWAIT
                trl->is_org &&
#endif
                (trl->org_or_adt == NULL || /* for option ptrs, init'd w/ NULL  */
                 ceu_org_is_cleared((tceu_org*)trl->org_or_adt,
                    (tceu_org*)((tceu_kill*)evt->param)->org_or_adt,
                    ((tceu_kill*)evt->param)->t1,
                    ((tceu_kill*)evt->param)->t2)))
        ||
#endif
#ifdef CEU_ADTS_AWAIT
            /* if */
            (evt->id==CEU_IN__ok_killed && trl->evt==CEU_IN__ok_killed &&
#ifdef CEU_ORGS_AWAIT
                !trl->is_org &&
#endif
                trl->org_or_adt == ((tceu_kill*)evt->param)->org_or_adt)
        ||
#endif
            /* if evt->id matches awaiting trail */
            (trl->evt==evt->id && trl->seqno!=app->seqno
#ifdef CEU_ORGS_OR_ADTS_AWAIT
                && (evt->id != CEU_IN__ok_killed)
                    /* TODO: simplify */
#endif
#ifdef CEU_INTS
#ifdef CEU_ORGS
                && (evt->id>=CEU_IN_lower || evt->org==trl->evto)
#endif
#endif
            )
           )
        {
            /*** CODE ***/
            trl->evt = CEU_IN__NONE;    /* TODO: dup w/ above */
            app->code(app, evt, org, trl, stk);
#ifdef CEU_STACK_CLEAR
            if (!stk->is_alive) {
                return;
            }
#endif

#if defined(CEU_OS_KERNEL) || defined(CEU_LUA)
            if (!app->isAlive) {
#ifdef CEU_OS_KERNEL
                CEU_GC = 1;
#endif
#ifdef CEU_LUA
                lua_close(app->lua);
#endif
            }
#endif
        }

        /* DON'T EXECUTE THIS TRAIL */
        else
        {
#ifdef CEU_DEBUG_TRAILS
SPC(1); printf("<<< NO\n");
#endif
#ifdef CEU_CLEAR
            if (evt->id==CEU_IN__CLEAR) {
                trl->evt = CEU_IN__NONE;    /* TODO: dup w/ above */
            }
#endif
        }

        /* NEXT TRAIL */

        if (trl->evt<=CEU_IN_higher && trl->seqno!=app->seqno) {
            trl->seqno = app->seqno-1;   /* keeps the gap tight */
        }
    }

#ifdef CEU_ORGS
    /* clearing the whole org? */
    if (evt->id==CEU_IN__CLEAR && org!=app->data && trl0==0 && trlF==org->n) {
        /* yes, relink and put it in the free list */
        ceu_sys_org_free(app, org);
    }
#endif
}

void ceu_sys_go_stk (tceu_app* app, int evt, void* evtp, tceu_stk* stk) {
    app->seqno++;
#ifdef CEU_DEBUG_TRAILS
    printf("===> [%d] %d\n", evt, app->seqno);
#endif

    switch (evt) {
#ifdef CEU_ASYNCS
        case CEU_IN__ASYNC:
            app->pendingAsyncs = 0;
            break;
#endif
#ifdef CEU_WCLOCKS
        case CEU_IN__WCLOCK:
            app->wclk_min_cmp = app->wclk_min_set;      /* swap "cmp" to last "set" */
            app->wclk_min_set = CEU_WCLOCK_INACTIVE;    /* new "set" resets to inactive */
            if (app->wclk_min_cmp <= *((s32*)evtp)) {
                app->wclk_late = *((s32*)evtp) - app->wclk_min_cmp;
            }
            break;
#ifdef CEU_TIMEMACHINE
        case CEU_IN__WCLOCK_:
            app->wclk_min_cmp_ = app->wclk_min_set_;
            app->wclk_min_set_ = CEU_WCLOCK_INACTIVE;
            if (app->wclk_min_cmp_ <= *((s32*)evtp)) {
                app->wclk_late_ = *((s32*)evtp) - app->wclk_min_cmp_;
            }
            break;
#endif
#endif
    }

    {
        tceu_evt evt_;
                 evt_.id = evt;
                 evt_.param = &evtp;
        ceu_sys_go_ex(app, &evt_,
                      stk,
                      app->data, 0,
#ifdef CEU_ORGS
                      app->data->n
#else
                      CEU_NTRAILS
#endif
        );
    }

#ifdef CEU_WCLOCKS
    if (evt==CEU_IN__WCLOCK) {
#ifdef ceu_out_wclock_set
        /* no new sets, signal inactive */
        if (app->wclk_min_set == CEU_WCLOCK_INACTIVE) {
            ceu_out_wclock_set(CEU_WCLOCK_INACTIVE);
        }
#endif
        app->wclk_late = 0;
    }
#ifdef CEU_TIMEMACHINE
    if (evt==CEU_IN__WCLOCK_) {
#ifdef ceu_out_wclock_set
        if (app->wclk_min_set_ == CEU_WCLOCK_INACTIVE) {
            ceu_out_wclock_set(CEU_WCLOCK_INACTIVE);
        }
#endif
        app->wclk_late_ = 0;
    }
#endif
#endif

#ifdef CEU_ORGS_NEWS_MALLOC
    while (app->tofree != NULL) {
        tceu_org* nxt = app->tofree->nxt;
        ceu_sys_realloc(app->tofree, 0);
        app->tofree = nxt;
    }
#endif
}

void ceu_sys_go (tceu_app* app, int evt, void* evtp)
{
#ifdef CEU_STACK_CLEAR
    tceu_stk stk_ = { NULL, NULL, 0, 0, 1 };
    ceu_sys_go_stk(app, evt, evtp, &stk_);
#else
    ceu_sys_go_stk(app, evt, evtp, NULL);
#endif
}

typedef struct {
    int    argc;
    char** argv;
} tceu_os_start;

int ceu_go_all (tceu_app* app, int argc, char **argv)
{
    /* All code run atomically:
     * - the program is always locked as a whole
     * -    thread spawns will unlock => re-lock
     * - but program will still run to completion
     */
    app->init(app);     /* calls CEU_THREADS_MUTEX_LOCK() */

#ifdef CEU_IN_OS_START
#if defined(CEU_RET) || defined(CEU_OS_KERNEL)
    if (app->isAlive)
#endif
    {
        tceu_os_start arg = { argc, argv };
        ceu_sys_go(app, CEU_IN_OS_START, &arg);
    }
#endif

#ifdef CEU_ASYNCS
    while(
#if defined(CEU_RET) || defined(CEU_OS_KERNEL)
            app->isAlive &&
#endif
            (
#ifdef CEU_THREADS
                app->threads_n>0 ||
#endif
                app->pendingAsyncs
            ) )
    {
        ceu_sys_go(app, CEU_IN__ASYNC, NULL);
#ifdef CEU_THREADS
        CEU_THREADS_MUTEX_UNLOCK(&app->threads_mutex);
        /* allow threads to also execute */
#if 0
        CEU_THREADS_YIELD();    /* should work with single cores? */
#endif
        usleep(100);            /* should work anyway */
        CEU_THREADS_MUTEX_LOCK(&app->threads_mutex);
#endif
    }
#endif

#ifdef CEU_THREADS
    CEU_THREADS_MUTEX_UNLOCK(&app->threads_mutex);
#endif

#ifdef CEU_NEWS
#ifdef CEU_RUNTESTS
    ceu_out_assert_msg(_ceu_dyns_ == 0, "memory leak");
#endif
#endif

#ifdef CEU_RET
    return app->ret;
#else
    return 0;
#endif
}

/**********************************************************************
 * Only the OS kernel needs any of the below.
 **********************************************************************/

#ifdef CEU_OS_KERNEL

/*
 * SYS_VECTOR:
 */
void* CEU_SYS_VEC[CEU_SYS_MAX] __attribute__((used)) = {
    (void*) &ceu_out_assert,
    (void*) &ceu_out_log,
    (void*) &ceu_sys_realloc,
    (void*) &ceu_sys_req,
    (void*) &ceu_sys_load,
#ifdef CEU_ISRS
    (void*) &ceu_sys_isr,
#endif
    (void*) &ceu_sys_org_init,
    (void*) &ceu_sys_start,
    (void*) &ceu_sys_link,
    (void*) &ceu_sys_unlink,
    (void*) &ceu_sys_emit,
    (void*) &ceu_sys_call,
#ifdef CEU_WCLOCKS
    (void*) &ceu_sys_wclock,
#endif
    (void*) &ceu_sys_go
};

/*****************************************************************************
 * QUEUE
 * - 256 avoids doing modulo operations
 * - n: number of entries
 * - 0: next position to consume
 * - i: next position to enqueue
 */
#if CEU_QUEUE_MAX == 256
    byte QUEUE[CEU_QUEUE_MAX] = {0};    /* {0} avoids .bss */
    int  QUEUE_tot = 0;
    u8   QUEUE_get = 0;
    u8   QUEUE_put = 0;
#else
    byte QUEUE[CEU_QUEUE_MAX] = {0};    /* {0} avoids .bss */
    int  QUEUE_tot = 0;
    u16  QUEUE_get = 0;
    u16  QUEUE_put = 0;
#endif

tceu_queue* ceu_sys_queue_get (void) {
    tceu_queue* ret;
    ceu_out_isr_off();
    if (QUEUE_tot == 0) {
        ret = NULL;
    } else {
#ifdef CEU_DEBUG
        ceu_sys_assert(QUEUE_tot > 0);
#endif
        ret = (tceu_queue*) &QUEUE[QUEUE_get];
    }
    ceu_out_isr_on();
    return ret;
}

int ceu_sys_queue_put (tceu_app* app, tceu_nevt evt, int sz, byte* buf) {
    ceu_out_isr_off();

    int n = sizeof(tceu_queue) + sz;

    if (QUEUE_tot+n > CEU_QUEUE_MAX) {
        return 0;   /* TODO: add event FULL when CEU_QUEUE_MAX-1 */
    }

    /* An event+data must be continuous in the QUEUE. */
    if (QUEUE_put+n+sizeof(tceu_queue)>=CEU_QUEUE_MAX && evt!=CEU_IN__NONE) {
        int fill = CEU_QUEUE_MAX - QUEUE_put - sizeof(tceu_queue);
        /*_ceu_sys_emit(app, CEU_IN__NONE, param, fill, NULL);*/
        tceu_queue* qu = (tceu_queue*) &QUEUE[QUEUE_put];
        qu->app = app;
        qu->evt = CEU_IN__NONE;
        qu->sz  = fill;
        QUEUE_put += sizeof(tceu_queue) + fill;
        QUEUE_tot += sizeof(tceu_queue) + fill;
    }

    {
        tceu_queue* qu = (tceu_queue*) &QUEUE[QUEUE_put];
        qu->app = app;
        qu->evt = evt;
        qu->sz  = sz;
        memcpy(qu->buf, buf, sz);
    }
    QUEUE_put += n;
    QUEUE_tot += n;

    ceu_out_isr_on();
    return 1;
}

void ceu_sys_queue_rem (void) {
    ceu_out_isr_off();
    tceu_queue* qu = (tceu_queue*) &QUEUE[QUEUE_get];
    QUEUE_tot -= sizeof(tceu_queue) + qu->sz;
    QUEUE_get += sizeof(tceu_queue) + qu->sz;
    ceu_out_isr_on();
}

/*****************************************************************************/

static tceu_app* CEU_APPS = NULL;
static tceu_lnk* CEU_LNKS = NULL;

#ifdef CEU_RET
    int ok  = 0;
    int ret = 0;
#endif

/* TODO: remove this indirection */
int ceu_sys_emit (tceu_app* app, tceu_nevt evt, int sz, void* param) {
    return ceu_sys_queue_put(app, evt, sz, param);
}

void* ceu_sys_call (tceu_app* app, tceu_nevt evt, void* param) {
    tceu_lnk* lnk = CEU_LNKS;
    for (; lnk; lnk=lnk->nxt)
    {
        if (app!=lnk->src_app || evt!=lnk->src_evt) {
            continue;
        }
        void* ret = lnk->dst_app->calls(lnk->dst_app, lnk->dst_evt, param);
        return ret;
    }
/* TODO: error? */
    return NULL;
}

static void _ceu_sys_unlink (tceu_lnk* lnk) {
    /* remove as head */
    if (CEU_LNKS == lnk) {
        CEU_LNKS = lnk->nxt;
/* TODO: prv */
    /* remove in the middle */
    } else {
        tceu_lnk* cur = CEU_LNKS;
        while (cur->nxt!=NULL && cur->nxt!=lnk) {
			cur = cur->nxt;
        }
        if (cur->nxt != NULL) {
            cur->nxt = lnk->nxt;
        }
	}

    /*lnk->nxt = NULL;*/
    ceu_sys_realloc(lnk, 0);
}

static void __ceu_os_gc (void)
{
    if (! CEU_GC) return;
    CEU_GC = 0;

    /* remove pending events */
    {
        ceu_out_isr_off();
        int i = 0;
        while (i < QUEUE_tot) {
            tceu_queue* qu = (tceu_queue*) &QUEUE[QUEUE_get+i];
            if (qu->app!=NULL && !qu->app->isAlive) {
                qu->evt = CEU_IN__NONE;
            }
            i += sizeof(tceu_queue) + qu->sz;
        }
        ceu_out_isr_on();
    }

    /* remove broken links */
    {
        tceu_lnk* cur = CEU_LNKS;
        while (cur != NULL) {
            tceu_lnk* nxt = cur->nxt;
            if (!cur->src_app->isAlive || !cur->dst_app->isAlive) {
                _ceu_sys_unlink(cur);
            }
            cur = nxt;
        }
    }

    /* remove dead apps */
    tceu_app* app = CEU_APPS;
    tceu_app* prv = NULL;
    while (app)
    {
        tceu_app* nxt = app->nxt;

        if (app->isAlive) {
            prv = app;

        } else {
            if (CEU_APPS == app) {
                CEU_APPS = nxt;     /* remove as head */
            } else {
                prv->nxt = nxt;     /* remove in the middle */
            }

            /* unlink all "from app" or "to app" */
            ceu_sys_unlink(app,0, 0,0);
            ceu_sys_unlink(0,0, app,0);

#ifdef CEU_RET
            ok--;
            ret += app->ret;
#endif

            /* free app memory */
            ceu_sys_realloc(app->data, 0);
            ceu_sys_realloc(app, 0);
        }

        app = nxt;
    }
}

#ifdef CEU_ISRS

typedef struct {
    tceu_isr_f f;
    tceu_app*  app;
} tceu_isr;

#define CEU_ISR_MAX 40
tceu_isr CEU_ISR_VEC[CEU_ISR_MAX];

int ceu_sys_isr (int n, tceu_isr_f f, tceu_app* app) {
    tceu_isr* isr = &CEU_ISR_VEC[(n-1)];
    if (f==NULL || isr->f==NULL) {
        isr->f   = ((word)app->addr>>1) + f;
        isr->app = app;
                           /* "f" is relative to "app", make it absolute */
        return 1;
    } else {
        return 0;
    }
}
#endif

void ceu_os_init (void) {
#ifdef CEU_ISRS
    int i;
    for (i=0; i<CEU_ISR_MAX; i++) {
        CEU_ISR_VEC[i].f = NULL;      /* TODO: is this required? (bss=0) */
    }
    ceu_out_isr_on();       /* enable global interrupts to start */
#endif
}

int ceu_os_scheduler (int(*dt)())
{
    /*
     * Intercalate DT->WCLOCK->ASYNC->QUEUE->...
     * QUEUE last to separate app->init() from OS_START.
     * QUEUE handles one event at a time to intercalate with WCLOCK.
     * __ceu_os_gc() only if QUEUE is emtpy: has to keep data from events 
     * accessible.
     */

#ifdef CEU_RET
    while (ok > 0)
#else
    while (1)
#endif
    {
#if defined(CEU_WCLOCKS) || defined(CEU_IN_OS_DT)
        s32 _dt = dt();
#endif

        /* DT */
#ifdef CEU_IN_OS_DT
        {
            tceu_app* app = CEU_APPS;
            while (app) {
                ceu_sys_go(app, CEU_IN_OS_DT, &dt);
                app = app->nxt;
            }
        }
#endif	/* CEU_IN_OS_DT */

        /* WCLOCK */
#ifdef CEU_WCLOCKS
        {
            tceu_app* app = CEU_APPS;
            while (app) {
/*
#error TODO: CEU_IN__WCLOCK_
*/
                ceu_sys_go(app, CEU_IN__WCLOCK, &_dt);
                app = app->nxt;
            }
        }
#endif	/* CEU_WCLOCKS */

        /* ASYNC */
#ifdef CEU_ASYNCS
        {
            tceu_app* app = CEU_APPS;
            while (app) {
                ceu_sys_go(app, CEU_IN__ASYNC, NULL);
                app = app->nxt;
            }
        }
#endif	/* CEU_ASYNCS */

        /* EVENTS */
        {
            /* clear the current size (ignore events emitted here) */
            ceu_out_isr_off();
            int tot = QUEUE_tot;
            ceu_out_isr_on();
            if (tot > 0)
            {
                tceu_queue* qu = ceu_sys_queue_get();
                tot -= sizeof(tceu_queue) + qu->sz;
                if (qu->evt == CEU_IN__NONE) {
                    /* nothing; */
                    /* "fill event" */

                /* global events (e.g. OS_START, OS_INTERRUPT) */
                } else if (qu->app == NULL) {
                    tceu_app* app = CEU_APPS;
                    while (app) {
                        ceu_sys_go(app, qu->evt, qu->buf);
                        app = app->nxt;
                    }

                } else {
                    /* linked events */
                    tceu_lnk* lnk = CEU_LNKS;
                    while (lnk) {
                        if ( qu->app==lnk->src_app
                        &&   qu->evt==lnk->src_evt
                        &&   lnk->dst_app->isAlive ) {
                            ceu_sys_go(lnk->dst_app, lnk->dst_evt, qu->buf);
                        }
                        lnk = lnk->nxt;
                    }
                }

                ceu_sys_queue_rem();
            }
            else
            {
                __ceu_os_gc();     /* only when queue is empty */
            }
        }
    }

#ifdef CEU_RET
    return ret;
#else
    return 0;
#endif
}

/* LOAD / START */

tceu_app* ceu_sys_load (void* addr)
{
    uint       size;
    tceu_init* init;
#ifdef CEU_OS_LUAIFC
    char*      luaifc;
#endif

    ((tceu_export) addr)(&size, &init
#ifdef CEU_OS_LUAIFC
                        , &luaifc
#endif
                        );

    tceu_app* app = (tceu_app*) ceu_sys_realloc(NULL, sizeof(tceu_app));
    if (app == NULL) {
        return NULL;
    }

    app->data = (tceu_org*) ceu_sys_realloc(NULL, size);
    if (app->data == NULL) {
        return NULL;
    }

    app->sys_vec = CEU_SYS_VEC;
    app->nxt = NULL;

    /* Assumes sizeof(void*)==sizeof(WORD) and
        that gcc will word-align SIZE/INIT */
    app->init = (tceu_init) ((word)init);
    app->addr = addr;

#ifdef CEU_OS_LUAIFC
    app->luaifc = luaifc;
#endif

    return app;
}

void ceu_sys_start (tceu_app* app)
{
    /* add as head */
	if (CEU_APPS == NULL) {
		CEU_APPS = app;

    /* add to tail */
    } else {
		tceu_app* cur = CEU_APPS;
        while (cur->nxt != NULL) {
            cur = cur->nxt;
        }
        cur->nxt = app;
    }

    /* MAX OK */
#ifdef CEU_RET
    ok++;
#endif

    /* INIT */

    app->init(app);

    /* OS_START */

#ifdef CEU_IN_OS_START
    ceu_sys_emit(NULL, CEU_IN_OS_START, 0, NULL);
#endif
}

/* LINK & UNLINK */

int ceu_sys_link (tceu_app* src_app, tceu_nevt src_evt,
                  tceu_app* dst_app, tceu_nevt dst_evt)
{
    tceu_lnk* lnk = (tceu_lnk*) ceu_sys_realloc(NULL, sizeof(tceu_lnk));
    if (lnk == NULL) {
        return 0;
    }

    lnk->src_app = src_app;
    lnk->src_evt = src_evt;
    lnk->dst_app = dst_app;
    lnk->dst_evt = dst_evt;
    lnk->nxt = NULL;

    /* add as head */
	if (CEU_LNKS == NULL) {
		CEU_LNKS = lnk;

    /* add to tail */
    } else {
		tceu_lnk* cur = CEU_LNKS;
        while (cur->nxt != NULL) {
            cur = cur->nxt;
        }
		cur->nxt = lnk;
    }

    return 1;
}

int ceu_sys_unlink (tceu_app* src_app, tceu_nevt src_evt,
                    tceu_app* dst_app, tceu_nevt dst_evt)
{
    tceu_lnk* cur = CEU_LNKS;
    while (cur != NULL) {
        tceu_lnk* nxt = cur->nxt;
        if ( (src_app==0 || src_app==cur->src_app)
          && (src_evt==0 || src_evt==cur->src_evt)
          && (dst_app==0 || dst_app==cur->dst_app)
          && (dst_evt==0 || dst_evt==cur->dst_evt) ) {
            _ceu_sys_unlink(cur);
        }
        cur = nxt;
    }
    return 0;
}

#ifdef CEU_ISRS

/* Foreach ISR, call ceu_sys_emit(CEU_IN_OS_INTERRUPT). */

#define GEN_ISR(n)                                                  \
    ISR(__vector_ ## n, ISR_BLOCK) {                                \
        tceu_isr* isr = &CEU_ISR_VEC[n-1];                          \
        if (isr->f != NULL) {                                       \
            CEU_APP_ADDR = isr->app->addr;                          \
            isr->f(isr->app, isr->app->data);                       \
            CEU_APP_ADDR = 0;                                       \
        }                                                           \
        ceu_sys_emit(NULL,CEU_IN_OS_INTERRUPT,CEU_EVTP(n),0,NULL); \
    }
#define _GEN_ISR(n)

GEN_ISR(20);
/*
GEN_ISR( 1) GEN_ISR( 2) GEN_ISR( 3) GEN_ISR( 4) GEN_ISR( 5)
GEN_ISR( 6) GEN_ISR( 7) GEN_ISR( 8) GEN_ISR( 9) GEN_ISR(10)
GEN_ISR(11) GEN_ISR(12) GEN_ISR(13) GEN_ISR(14) GEN_ISR(15)
GEN_ISR(16) GEN_ISR(17) _GEN_ISR(18) GEN_ISR(19) GEN_ISR(20)
GEN_ISR(21) GEN_ISR(22) GEN_ISR(23) GEN_ISR(24) GEN_ISR(25)
GEN_ISR(26) GEN_ISR(27) GEN_ISR(28) GEN_ISR(29) GEN_ISR(30)
GEN_ISR(31) GEN_ISR(32) GEN_ISR(33) GEN_ISR(34) GEN_ISR(35)
GEN_ISR(36) GEN_ISR(37) GEN_ISR(38) GEN_ISR(39) GEN_ISR(40)
*/

#endif /* CEU_ISRS */

#endif /* ifdef CEU_OS_KERNEL */

#endif /* ifndef CEU_OS_APP */
