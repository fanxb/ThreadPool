#ifndef PTHREAD_POOL_H
#define PTHREAD_POOL_H


#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>


#define handle_error_en(en, msg) \
              do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

//等待线程池中的所有非空闲任务执行完成然后销毁线程池
#define WAIT_THREAD_FINSISH




//线程任务链表结构
typedef struct thread_worker
{
  void* (*callback_function)(void *arg);    //线程回调函数
  void *arg;                              //回调函数参数
  struct thread_worker *next;
}thread_worker_st;

//线程池结构
typedef struct thread_pool
{
  pthread_mutex_t mutex_lock;           //队列互斥锁
  pthread_cond_t  queue_not_empty;          //队列条件变量
  pthread_cond_t  queue_empty;          //队列为空条件变量（销毁线程池时用于等待非空闲线程退出）
  pthread_cond_t  queue_not_full;       //队列不为满的条件变量

  thread_worker_st *worker_head;        //任务队列头指针
  thread_worker_st *worker_tail;
  bool isdestroy;                       //线程池是否已经销毁
  pthread_t *worker_thread_id;          //线程ID数组
  uint16_t thread_num;					        //线程池中开启线程的个数
  uint16_t queue_max_num;					      //队列中最大worker的个数
  uint16_t queue_cur_num;               //队列当前的worker个数
}thread_pool_st;

/*****************************************************************************
*函数名：thread_pool_init
*描述：线程池初始化
*输入参数：
*       uint16_t thread_num;					        //线程池中开启线程的个数
*       uint16_t queue_max_num;					      //队列中最大worker的个数
*输出参数：
        无
*返回值：
      成功:线程池地址
      失败：NULL
****************************************************************************/
thread_pool_st* thread_pool_init(uint16_t thread_num, uint16_t queue_max_num);



/******************************************************************************
*函数名：create_detach_thread
*描述：线程池中线程函数
*输入参数：
*       thread_pool_st *thread_pool
*       uint16_t thread_idx
*输出参数：
        无
*返回值：
*      成功:0
*      失败：-1
****************************************************************************/
static int32_t create_detach_thread(thread_pool_st *thread_pool, uint16_t thread_idx);


/******************************************************************************
*函数名：thread_pool_add_worker
*描述：向线程池中添加任务
*输入参数：
*       struct thread_pool_st *thread_pool					//线程池地址
*       void* (*callback_function)(void *arg)				//线程池回调函数
*       void *arg)                                  //回调函数参数
*输出参数：
        无
*返回值：
      成功:0
      失败：-1
******************************************************************************/
  uint32_t thread_pool_add_worker(thread_pool_st *thread_pool,
                                void* (*callback_function)(void *arg),
                                void *arg);




/***************************************************************************
*函数名：thread_pool_function
*描述：线程池中线程函数
*输入参数：
*        void* arg
*输出参数：
*        无
*返回值：
*      void*
****************************************************************************/
void* thread_pool_function(void* arg);



/****************************************************************************
*函数名：thread_pool_destroy
*描述：线程池销毁
*输入参数：
*        struct thread_pool_st *thread_pool					//线程池地址
*输出参数：
*        无
*返回值：
*      成功:0
*      失败：-1
*****************************************************************************/
uint32_t thread_pool_destroy(thread_pool_st *thread_pool);






#endif
