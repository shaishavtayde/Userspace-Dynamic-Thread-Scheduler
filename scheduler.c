/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scheduler.c
 */

#undef _FORTIFY_SOURCE

#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include "system.h"
#include "scheduler.h"

/**
 * Needs:
 *   setjmp()
 *   longjmp()
 */

/* research the above Needed API and design accordingly */

#define SZ_STACK 4096

struct thread {
    jmp_buf env; /* acts as a bookmark, saves the state of the thread*/
    enum {
        THREAD_NOT_STARTED,
        THREAD_RUNNING,
        THREAD_SLEEPING,
        THREAD_TERMINATED
    } status;
    struct {
        void memory_; / where the stack memory is allocated */
        void memory; / Aligned memory , which the thread will use */
    } stack;
    scheduler_fnc_t fnc;
    void *arg;
    struct thread *next;
};

static struct {
    struct thread *head;
    struct thread *current;
    jmp_buf env;
} scheduler;

struct thread *thread_candidate(void) {
    struct thread *curr = scheduler.current->next;
    while (curr != scheduler.current) {
        if (curr->status != THREAD_TERMINATED) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/* Fucntion to allocate and initialize thread*/
static struct thread* allocate_thread(scheduler_fnc_t fnc, void *arg) {
    struct thread *thread = malloc(sizeof(struct thread));
    if (!thread) {
        TRACE("malloc failed\n");
        return NULL;
    }
    thread->fnc = fnc;
    thread->arg = arg;
    thread->status = THREAD_NOT_STARTED;
    thread->next = NULL;
    return thread;
}

/* Allocate and align stack memory */
static int setup_stack(struct thread *thread) {
    size_t PAGE_SIZE = page_size();

    thread->stack.memory_ = malloc(SZ_STACK + PAGE_SIZE);
    if (!thread->stack.memory_) {
        TRACE("malloc failed\n");
        return -1;
    }

    thread->stack.memory = memory_align(thread->stack.memory_, PAGE_SIZE);
    return 0;
}

/* Helper function to insert a thread into the scheduler's linked list */
static void insert_thread_into_scheduler(struct thread *thread) {
    if (scheduler.head == NULL) {
        scheduler.head = thread;
        scheduler.current = thread;
        thread->next = thread;
    } else {
        struct thread *current = scheduler.current;
        struct thread *next = scheduler.current->next;
        current->next = thread;
        thread->next = next;
    }
}


int scheduler_create(scheduler_fnc_t fnc, void *arg) {
    struct thread *thread = allocate_thread(fnc, arg);
    if (!thread) {
        return -1; 
    }

    if (setup_stack(thread) < 0) {
        free(thread);
        return -1; 
    }

    insert_thread_into_scheduler(thread);
    return 0;
}


/* select the next candidate thread */
static struct thread* select_next_thread(void) {
    struct thread *candidate = thread_candidate();
    return candidate;
}

/* set up the stack pointer for a new thread */
static void setup_stack_pointer(struct thread *thread) {
    uint64_t rsp = (uint64_t) thread->stack.memory + SZ_STACK;
    _asm_ volatile ("mov %[rs], %%rsp \n" : [rs] "+r"(rsp) ::);
}

/* running a new thread */
static void run_new_thread(struct thread *thread) {
    thread->status = THREAD_RUNNING;
    thread->fnc(thread->arg);
    thread->status = THREAD_TERMINATED;
}


void schedule(void) {
    struct thread candidate = select_next_thread(); / calls the helper function to choose next thread */
    if (candidate == NULL) {
        return; 
    }

    scheduler.current = candidate;  /* assigns scheduler current to the selected thread */

    if (scheduler.current->status == THREAD_NOT_STARTED) {
        
        setup_stack_pointer(scheduler.current);
        run_new_thread(scheduler.current);
        longjmp(scheduler.env, 1);  /* Return to the scheduler saved position - inital here */
    } else {
        
        scheduler.current->status = THREAD_RUNNING;
        longjmp(scheduler.current->env, 1); /* Returns to current scheduler state*/
    }
}



static void free_thread_memory(struct thread *thread) {
    free(thread->stack.memory_);
    free(thread);
}

/*Free all threads in the scheduler */
static void free_all_threads(void) {
    struct thread *curr = scheduler.current->next;
    while (curr != scheduler.current) {
        struct thread *next = curr->next;
        free_thread_memory(curr);
        curr = next;
    }
}

/* reset the scheduler state */
static void reset_scheduler(void) {
    free_thread_memory(scheduler.current);
    scheduler.current = NULL;
    scheduler.head = NULL;
}


void destroy(void) {
    if (scheduler.current == NULL) {
        return; /* No threads to destroy */
    }

    free_all_threads();
    reset_scheduler();
}


void set_timer(void) {
    if (SIG_ERR == signal(SIGALRM, (__sighandler_t) scheduler_yield)) {
        TRACE("signal()");
    }
    alarm(1);
}

void clear_timer(void) {
    alarm(0);
    if (SIG_ERR == signal(SIGALRM, SIG_DFL)) {
        TRACE("signal()");
    }
}

void scheduler_execute(void) {
    setjmp(scheduler.env);
    set_timer();
    schedule();
    clear_timer();
    destroy();
}

/* To put the current thread to sleep */
static void put_thread_to_sleep(void) {
    scheduler.current->status = THREAD_SLEEPING;
}


void scheduler_yield(int signum) {
    assert(SIGALRM == signum);

    if (setjmp(scheduler.current->env) == 0) {
        put_thread_to_sleep();
        longjmp(scheduler.env, 1); 
    }
}