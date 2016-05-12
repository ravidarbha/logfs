#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include <linux/hash.h>
#include <linux/delay.h>
#include <linux/slab.h>

// This fle is taking care of all the worker thread related functions.

typedef struct __rubix_condvar_t rubix_condvar_t;
// Align the condvar to 8 bytes
struct  __attribute__((aligned(8))) __rubix_condvar_t {
    char dummy[152];
};

typedef struct rubix_worker_struct_
{
    rubix_condvar_t cv;
    int done;
    struct work_struct wq;
}rbx_worker_t;


///////////////////////////////////  SYNC PRIMITIVES //////////////////////////////////

// ONly one thread.
void condvar_signal(rubix_condvar_t *cv)
{
    wait_queue_head_t *q = (wait_queue_head_t *)cv;

    // Wake up on the wait queue.
    wake_up(q);
}

//Wakeup all threads.
void condvar_broadcast(rubix_condvar_t *cv)
{
    wait_queue_head_t *q = (wait_queue_head_t *)cv;
    // Wake up all on the wait queue.
    wake_up_all(q);
}

void condvar_wait(rubix_condvar_t *cv)
{
    wait_queue_head_t *q = (wait_queue_head_t *)cv;

    DEFINE_WAIT(_wait);

    prepare_to_wait_exclusive(q, &_wait, TASK_INTERRUPTIBLE);
    schedule();

    finish_wait(q, &_wait);
}

void condvar_init(rubix_condvar_t *cv)
{
    wait_queue_head_t *q = (wait_queue_head_t *)cv;

    init_waitqueue_head(q);
}

//////////////////////////////////////////// SYNC PRIMITIVES /////////////////////////////////


// We ll have to make this a heap allocation variable.
// This wakes up, does some work and signals the sleepers.
// The main deferrable function for our worker threads.
void thread_function(struct work_struct *work)
{
    rbx_worker_t *rbx_work =  container_of(work, rbx_worker_t, wq);
    printk("In thread function ..\n");
    rbx_work->done=1;
    condvar_signal(&rbx_work->cv);
}

void create_worker(rbx_worker_t *rbx_work)
{
    // Dont bind anything to the cpu as of now.
    INIT_WORK((struct work_struct *) &rbx_work->wq, (work_func_t) thread_function);
 
    schedule_work((struct work_struct *)&rbx_work->wq);
 
    while(!rbx_work->done)
    {
        condvar_wait(&rbx_work->cv);
    }
    printk("Done with the thread..\n");
}

void *init_worker_threads(void)
{
    rbx_worker_t *rbx_work = NULL;

    rbx_work = (rbx_worker_t *)kmalloc((sizeof(*rbx_work)),GFP_KERNEL); 
    if (rbx_work)
    {
        condvar_init(&rbx_work->cv);
 
        rbx_work->done = 0;
        // Maybe this can be calld whenever we want workers.
        create_worker(rbx_work);
    }
    return (void *)rbx_work;
}

void destroy_worker_threads(void *rbx_)
{
    rbx_worker_t *rbx_work = (rbx_worker_t *)rbx_;
    kfree(rbx_work);
}
