#include "pthread_pool.h"
#include <unistd.h>



void* myprocess(void *arg)
{
  char *str = (char *)arg;
  printf("%stid: %d\n", str, pthread_self());
}

int main()
{
  thread_pool_st *thread_pool = NULL;
  thread_pool = thread_pool_init(10, 5);
  char *str = "ThreadPool_";

  int i;
  for (i = 0; i < 8; i++)
  {
    thread_pool_add_worker(thread_pool, myprocess, (void *)str);
  }

  printf("main_thread_ID:%d\n",pthread_self());

  while(1)
  {
    sleep(2);
  }
  return 0;
}
