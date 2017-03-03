#include "pthread_pool.h"
thread_pool_st* thread_pool_init(uint16_t thread_num, uint16_t queue_max_num)
{
  thread_pool_st *thread_pool = NULL;
  thread_pool = (thread_pool_st *)malloc(sizeof(thread_pool_st));
  if(NULL == thread_pool)
  {
    printf("%s:%d\n", __FILE__, __LINE__);
    perror("malloc()");
    return NULL;
  }

  //初始化锁
  if (pthread_mutex_init(&(thread_pool->mutex_lock), NULL))
  {
      printf("%s:%d\n", __FILE__, __LINE__);
      perror("pthread_mutex_init()");
      return NULL;
  }

  //初始化条件变量
  if (pthread_cond_init(&(thread_pool->queue_not_empty), NULL))
  {
      printf("%s:%d\n", __FILE__, __LINE__);
      perror("pthread_cond_init()");
      return NULL;
  }
  if (pthread_cond_init(&(thread_pool->queue_empty), NULL))
  {
      printf("%s:%d\n", __FILE__, __LINE__);
      perror("pthread_cond_init()");
      return NULL;
  }
  if (pthread_cond_init(&(thread_pool->queue_not_full), NULL))
  {
      printf("%s:%d\n", __FILE__, __LINE__);
      perror("pthread_cond_init()");
      return NULL;
  }

  thread_pool->worker_head = NULL;
  thread_pool->worker_tail = NULL;
  thread_pool->isdestroy = false;

  //线程池中线程标识数组
  thread_pool->worker_thread_id = (pthread_t * )malloc(sizeof(pthread_t) * thread_num);
  if (NULL == thread_pool->worker_thread_id)
  {
      printf("%s:%d\n", __FILE__, __LINE__);
      perror("malloc()");
      return NULL;
  }

  thread_pool->thread_num = thread_num;
  thread_pool->queue_max_num = queue_max_num;
  thread_pool->queue_cur_num = 0;

  int thread_idx;
  for (thread_idx = 0; thread_idx < thread_pool->thread_num; thread_idx++)
  {
      //创建分离线程
      create_detach_thread(thread_pool, thread_idx);
  }

  return thread_pool;

}

static int32_t create_detach_thread(thread_pool_st *thread_pool, uint16_t thread_idx)
{
  int32_t ret;
  pthread_attr_t attr;
  ret = pthread_attr_init(&attr);
  if (0 != ret)
  {
    printf("%s:%d\n", __FILE__, __LINE__);
    handle_error_en(ret, "pthread_attr_init()");
    return -1;
  }

  ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (0 != ret)
  {
    printf("%s:%d\n", __FILE__, __LINE__);
    handle_error_en(ret, "pthread_attr_setdetachstate()");
    return -1;
  }

  ret = pthread_create(&(thread_pool->worker_thread_id[thread_idx]),
                       &attr, thread_pool_function, (void*)thread_pool);
   if (0 != ret)
   {
     printf("%s:%d\n", __FILE__, __LINE__);
     handle_error_en(ret, "pthread_create()");
     pthread_attr_destroy(&attr);
     return -1;
   }

   //pthread_attr_destroy(&attr);

   return 0;
}
uint32_t thread_pool_add_worker(thread_pool_st *thread_pool,
                                void* (*callback_function)(void *arg),
                                void *arg)
{
  thread_worker_st *thread_worker = NULL;
  thread_worker = (thread_worker_st *)malloc(sizeof(thread_worker_st));
  if (NULL == thread_worker)
  {
      printf("%s:%d\n", __FILE__, __LINE__);
      perror("malloc()");
      return -1;
  }
  thread_worker->callback_function = callback_function;
  thread_worker->arg = arg;
  thread_worker->next = NULL;

  //任务队列已满
  pthread_mutex_lock(&(thread_pool->mutex_lock));
  while (thread_pool->queue_cur_num == thread_pool->queue_max_num)
  {
    pthread_cond_wait(&(thread_pool->queue_not_full), &(thread_pool->mutex_lock));   //队列满的时候就等待
  }

  //向任务链表添加新任务
  if (thread_pool->worker_head == NULL)
  {
      thread_pool->worker_head = thread_worker;
      thread_pool->worker_tail = thread_worker;
  }
  else
  {
      //将新任务加入任务链表的头部
      // thread_worker->next = thread_pool->worker_head;
      // thread_pool->worker_head = thread_worker;

      thread_pool->worker_tail->next = thread_worker;
      thread_pool->worker_tail = thread_worker;
  }
  thread_pool->queue_cur_num++;
  pthread_mutex_unlock(&(thread_pool->mutex_lock));
  pthread_cond_signal(&(thread_pool->queue_not_empty));//激活一个线程池中的空闲线程执行此任务


  return 0;
}


void* thread_pool_function(void* arg)
{
  thread_pool_st *thread_pool = (thread_pool_st *)arg;
  thread_worker_st *thread_worker = NULL;

  //任务执行完成后继续等待下一个任务
  while (1)
  {
      //线程队列为空等待队列添加任务
      pthread_mutex_lock(&(thread_pool->mutex_lock));
      
      //防止载多核处理器上产生虚假唤醒
      //Some implementations, particularly on a multi-processor, may sometimes cause multiple threads to wake up when the condition variable is signaled  simultaneously  on
      // different processors.
      while (0 == thread_pool->queue_cur_num)
      {
         pthread_cond_wait(&(thread_pool->queue_not_empty), &(thread_pool->mutex_lock));
      }

      //线程池销毁时执行使这个空闲线程退出
      if (true == thread_pool->isdestroy)
      {
          pthread_mutex_unlock(&(thread_pool->mutex_lock));
          pthread_exit(EXIT_SUCCESS);
      }

      //取走任务链表里的任务
      thread_pool->queue_cur_num--;
      thread_worker = thread_pool->worker_head;
      thread_pool->worker_head = thread_worker->next;

      //队列为空,通知销毁线程函
      if (thread_pool->queue_cur_num == 0)
      {
          pthread_cond_signal(&(thread_pool->queue_empty));
      }

      //队列非满，通知添加函数，添加新任务
      if (thread_pool->queue_cur_num == thread_pool->queue_max_num - 1)
      {
          pthread_cond_signal(&(thread_pool->queue_not_full));
      }

      pthread_mutex_unlock(&(thread_pool->mutex_lock));

      //线程真正要做的工作，回调函数的调用
      (*(thread_worker->callback_function))(thread_worker->arg);
      //任务执行结束后释放任务链表已经取走的任务节点
      free(thread_worker);
      thread_worker = NULL;

  }
}


uint32_t thread_pool_destroy(thread_pool_st *thread_pool)
{
  int32_t pthread_idx;
  int32_t ret;

  pthread_mutex_lock(&(thread_pool->mutex_lock));
  //线程池已销毁
  if (true == thread_pool->isdestroy)
  {
    pthread_mutex_unlock(&(thread_pool->mutex_lock));
    return -1;
  }

#ifdef WAIT_THREAD_FINSISH
  //等待线程池中队列任务链表中的任务全部被完成
  while (thread_pool->queue_cur_num != 0)
  {
      pthread_cond_wait(&(thread_pool->queue_empty), &(thread_pool->mutex_lock));
  }

  //激活所有阻塞任务，使线程池中所有空闲任务退出
  thread_pool->isdestroy = true;
  pthread_cond_broadcast(&(thread_pool->queue_not_empty));

  //等待线程池中所有非空闲任务执行完成并退出
  for (pthread_idx = 0; pthread_idx < thread_pool->thread_num; pthread_idx++)
  {
      ret = pthread_join(thread_pool->worker_thread_id[pthread_idx], NULL);    //等待线程池的所有线程执行完毕
      if (0 != ret)
      {
        if (ESRCH == ret)
          continue;//线程号不存在
        else
        {
          handle_error_en(ret, "pthread_attr_init()");
          return -1;
        }
      }
  }

#else
  //激活所有阻塞任务，使线程池中所有空闲任务退出
  thread_pool->isdestroy = true;
  pthread_cond_broadcast(&(thread_pool->queue_not_empty));

  //不等待，直接杀调线程池中非空闲线程
  for (pthread_idx = 0; pthread_idx < thread_pool->thread_num; pthread_idx++)
  {
    ret = pthread_kill(thread_pool->worker_thread_id[pthread_idx], 0);
    if (0 != ret)
    {
      if (ESRCH == ret)
        continue;//线程号不存在
      else
      {
        handle_error_en(ret, "pthread_attr_init()");
        return -1;
      }
    }

  }

  //释放线程任务队列内存
  thread_worker_st *ptr = NULL;
  while (NULL != thread_pool->worker_head)
  {
    ptr = thread_pool->worker_head;
    thread_pool->worker_head = ptr->next;
    free(ptr);
  }

#endif

  pthread_mutex_destroy(&(thread_pool->mutex_lock));
  pthread_cond_destroy(&(thread_pool->queue_not_empty));
  pthread_cond_destroy(&(thread_pool->queue_empty));
  pthread_cond_destroy(&(thread_pool->queue_not_full));

  //释放线程ID数组内存
  free(thread_pool->worker_thread_id);
  thread_pool->worker_thread_id = NULL;

  free(thread_pool);
}
