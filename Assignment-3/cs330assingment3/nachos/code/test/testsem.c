#include "syscall.h"
#include "synchop.h"

int
main()
{
    int x,y,i,id;
    int *a = (int*) system_ShmAllocate(sizeof(int));
    int b = 1;
    a[0]=0;
    id = system_SemGet(10);
    system_SemCtl(id,SYNCH_SET,&b);
    system_SemCtl(id,SYNCH_GET,&b);
    system_PrintString("Value of semaphore:");
    system_PrintInt(b);
    system_PrintChar('\n');
    x = system_Fork();
    if (x == 0) {
       y = system_Fork();
       if (y == 0) {
          for(i=0;i<100;i++){
	 	system_SemOp(id,-1);
          	a[0]++;
                system_SemOp(id,1);
          }
       }
       else {
          for(i=0;i<100;i++) {
		system_SemOp(id,-1);
          	a[0]++;
		system_SemOp(id,1);
	  }
          system_Join(y);
       }
    }
    else { 
	for(i=0;i<100;i++) {
		system_SemOp(id,-1);
        	a[0]++;
		system_SemOp(id,1);
	}
        system_Join(x);
	system_SemCtl(id,SYNCH_REMOVE,&b);
    }
    system_PrintString(" Final value:");
    system_PrintInt(a[0]);
    system_PrintChar('\n');
    return 0;
}
