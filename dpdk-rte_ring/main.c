#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <rte_ring.h>

static int g_ring_size = 8192;
static int g_prod_ptherad_num = 8;
static int g_cons_ptherad_num = 8;

static pthread_mutex_t g_workers_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_completed_num = 0;

typedef struct cc_queue_node 
{
    int data;
} cc_queue_node_t;

static struct rte_ring *r;
typedef struct rte_ring_task
{
    double tps;
}task;

task *g_prod_work_task = NULL;
task *g_cons_work_task = NULL;

void *enqueue_fun(void *arg)
{
    task *p_task = (task *)arg;
    if ( !p_task )
    {
        return NULL;
    }
    clock_t start,end;
    double tps;
    int n = g_ring_size;
    int i = 1;
    int ret;
    cc_queue_node_t *p;

    start = clock();
    for (; i < n; i++)
    {
        p = (cc_queue_node_t *)malloc(sizeof(cc_queue_node_t));
        p->data = i;
        ret = rte_ring_enqueue(r, p);
        if (ret != 0)
        {
            printf("enqueue failed: %d\n", i);
        }
    }
    end = clock();
    p_task->tps = (n*CLOCKS_PER_SEC/(end - start));
    pthread_mutex_lock(&g_workers_lock);
    g_completed_num++;
    pthread_mutex_unlock(&g_workers_lock);
    return NULL;
}

void *dequeue_func(void *arg)
{
    task *p_task = (task *)arg;
    int ret;
    int i = 1;
    int n = g_ring_size;
    cc_queue_node_t *p;
    clock_t start,end;
    double tps;

    start = clock();
    while (1) 
    {
        p = NULL;
        ret = rte_ring_dequeue(r, (void **)&p);
        if (ret != 0) 
        {
            printf("rte_ring_sc_dequeue error\n");
        }
        if (p != NULL) 
        {
            i++;
            free(p);
            if (i == n) 
            {
                break;
            }
        }
    }

    end = clock();
    p_task->tps = (n*CLOCKS_PER_SEC/(end - start));
    pthread_mutex_lock(&g_workers_lock);
    g_completed_num++;
    pthread_mutex_unlock(&g_workers_lock);

    return NULL;
}

static int parse_args(int argc, char **argv)
{
    int opt = -1;
    while (-1 != (opt = getopt(argc, argv, "p:s:")))
    {
        switch (opt)
        {
            case 'p':
                int pthread = atoi(optarg);
                if ( pthread < 0)
                    return -1;
                g_prod_ptherad_num = g_cons_ptherad_num = pthread;
                break;
            case 's':
                int num = atoi(optarg);
                if ( num < 0)
                    return -1;
                g_ring_size = num;
                break;
            default:
                break;
        }
    }

    return opt;
}

void dump_user_config(void)
{
    printf("=========================================================================\n");
    printf("TEST Configuration:\n");
    printf("PROD_PTHREAD_NUM: %d\n",g_prod_ptherad_num);
    printf("CONS_PTHREAD_NUM: %d\n",g_cons_ptherad_num);
    printf("Each thread enqueue:%5d\n",g_ring_size);
    return;
}

int dump_result(void)
{
    printf("==================================enqueue===================================\n");
    printf("\nThread    TPS     \n");
    for (int i=0; i<g_prod_ptherad_num; i++)
    {
        printf("%d   %16.1f\n", i,  g_prod_work_task[i].tps);
    }
    printf("==================================dequeue===================================\n");
    printf("\nThread    TPS     \n");
    for (int i=0; i<g_cons_ptherad_num; i++)
    {
        printf("%d   %16.1f\n", i,  g_cons_work_task[i].tps);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    pthread_mutex_init(&g_workers_lock, NULL);    
    int ret = 0;
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
    {
        rte_exit(EXIT_FAILURE, "Cannot init EAL\n");
    }
                
    pthread_t pid1;
    pthread_attr_t pthread_attr;
    int total_ring_size = g_ring_size * g_prod_ptherad_num;
    r = rte_ring_create("test", total_ring_size, SOCKET_ID_ANY, 0);
  
    if (r == NULL)
    {
        return -1;
    }

    g_prod_work_task = (task *)calloc(g_prod_ptherad_num, sizeof(task));
    g_cons_work_task = (task *)calloc(g_prod_ptherad_num, sizeof(task));
    
    if (!g_prod_work_task || !g_cons_work_task)
    {
        printf("Create prod or cons work task faild\n");
    }

    pthread_attr_init(&pthread_attr);
    int i;
    for (i=0; i<g_prod_ptherad_num; i++)
    {
        pthread_t pid1;
        if ((ret = pthread_create(&pid1, &pthread_attr, enqueue_fun, (void *)&g_prod_work_task[i]) == 0)) 
        {
            pthread_detach(pid1);
        }
        else
        {
            printf("prod pthread create faild\n");
            return -1;
        } 
    }
    while(1)
    {
        if (g_completed_num == g_prod_ptherad_num)
        {
            break;
        }
        sleep(1);
    }

    pthread_attr_init(&pthread_attr);
    for (i=0; i<g_cons_ptherad_num; i++)
    {
        pthread_t pid2;
        if ((ret = pthread_create(&pid2, &pthread_attr, dequeue_func, (void *)&g_cons_work_task[i]) == 0)) 
        {
            pthread_detach(pid2);
        }
    }
    g_completed_num = 0;
    while(1)
    {
        if (g_completed_num == g_cons_ptherad_num)
        {
            break;
        }
        sleep(1);
    } 

    rte_ring_free(r);
    dump_user_config();
    dump_result();
    pthread_mutex_destroy(&g_workers_lock);
    free(g_prod_work_task);
    free(g_cons_work_task);
    return 0;
}
