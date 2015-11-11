#include "syscall.h"
#include "synchop.h"

int
main()
{
     int x,y,i,idC,idS;
     int *a = (int*) system_ShmAllocate(sizeof(int));
     int b =1;
     a[0] = 0;
     idC = system_CondGet(10);
     idS = system_SemGet(11);
     system_SemCtl(idS,SYNCH_SET,&b);
     x = system_Fork();
     if (x==0) {
        y = system_Fork();
        if (y == 0) {
           system_SemOp(idS,-1);
           for (i =0;i<100;i++) {
               system_CondOp(idC,COND_OP_WAIT,idS);
               a[0]++;
               system_CondOp(idC,COND_OP_SIGNAL,0);
           }
           system_SemOp(idS,1);
        }
        else {
	   system_SemOp(idS,-1);
           for (i =0;i<100 ; i++) {
	       system_CondOp(idC,COND_OP_WAIT,idS);
               a[0]++;
		system_CondOp(idC,COND_OP_SIGNAL,0);
           }
	   system_SemOp(idS,1);
           system_Join(y);
        }
     }
     else {
	system_SemOp(idS,-1);
        for (i=0;i<100;i++) {
	   system_CondOp(idC,COND_OP_WAIT,idS);
           a[0]++;
	  system_CondOp(idC,COND_OP_SIGNAL,idS);
        }
	system_SemOp(idS,1);
        system_Join(x);
        system_CondRemove(idC);
     }
     system_PrintString("Final Value:");
     system_PrintInt(a[0]);
     system_PrintChar('\n');
     return 0;
}
