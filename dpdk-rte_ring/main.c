#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <rte_ring.h>

#define RING_SIZE 1024
static int g_prod_ptherad_num = 10;
static int g_cons_ptherad_num = 1;
static int g_prod_quene_size  = 1024;
typedef struct cc_queue_node {
    int data;
} cc_queue_node_t;

static struct rte_ring *r;

typedef unsigned long long ticks;

static __inline__ ticks getticks(void)
{
    u_int32_t a, d;

    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    return (((ticks)a) | (((ticks)d) << 32));
}


void *enqueue_fun(void *arg)
{
    int n = g_prod_quene_size;
    int i = 0;
    int ret;
    cc_queue_node_t *p;

    for (; i < n; i++) {
        p = (cc_queue_node_t *)malloc(sizeof(cc_queue_node_t));
        p->data = i;
        ret = rte_ring_enqueue(r, p);
        if (ret != 0) {
            printf("enqueue failed: %d\n", i);
        }
    }

    return NULL;
}

void *dequeue_func(void *arg)
{
    int ret;
    int i = 0;
    int sum = 0;
    int n = g_prod_quene_size * g_prod_ptherad_num;
    cc_queue_node_t *p;
    ticks t1, t2, diff;

    t1 = getticks();
    while (1) {
        p = NULL;
        ret = rte_ring_dequeue(r, (void **)&p);
        if (ret != 0) {
            printf("rte_ring_sc_dequeue error\n");
        }
        if (p != NULL) {
            i++;
            sum += p->data;
            free(p);
            if (i == n) {
                break;
            }
        }
    }

    t2 = getticks();
    diff = t2 - t1;
    printf("time diff: %llu\n", diff);
    printf("dequeue total: %d, sum: %d\n", i, sum);

    return NULL;
}


int main(int argc, char *argv[])
{
        
    int ret = 0;
    pthread_t pid1;
    pthread_attr_t pthread_attr;

    r = rte_ring_create("test", RING_SIZE, SOCKET_ID_ANY, 0);
  
    if (r == NULL)
    {
        return -1;
    }
    printf("start enqueue, 5 producer threads, echo thread enqueue 1000 numbers.\n");
    pthread_attr_init(&pthread_attr);
    int i;
    for (i=0; i<g_prod_ptherad_num; i++)
    {
        pthread_t pid1;
        if ((ret = pthread_create(&pid1, &pthread_attr, enqueue_fun, NULL) == 0)) 
        {
            pthread_detach(pid1);
        }
        else
        {
            printf("prod pthread create faild\n");
            return -1;
        } 
    }

    printf("start dequeue, 1 consumer thread.\n");
    pthread_t pid2;
    if ((ret = pthread_create(&pid2, &pthread_attr, dequeue_func, NULL))) 
    {
         printf("cons pthread create faild\n");
    }    
    pthread_join(pid2, NULL);

    rte_ring_free(r);

 
    return 0;
}
