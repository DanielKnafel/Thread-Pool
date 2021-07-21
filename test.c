#include <stdio.h>
#include <stdlib.h>
#include "osqueue.h"
#include "threadPool.h"
#include <unistd.h>

void hello (void* a)
{
   printf("hello from %lu\n", pthread_self());
}


void test_thread_pool_sanity()
{
   int i;
   
   ThreadPool* tp = tpCreate(5);
   for(i=0; i<5; ++i)
   {
      tpInsertTask(tp,hello,NULL);
   }
   
   tpDestroy(tp,1);
}


int main()
{
   test_thread_pool_sanity();

   return 0;
}
