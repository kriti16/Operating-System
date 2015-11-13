#include "syscall.h"

int
main()
{
    int x;
    int sleep_start, sleep_end;
    //int *a = (int*) system_ShmAllocate(sizeof(int));
    //a[0]=9876;
    system_PrintString("Parent PID: ");
    system_PrintInt(system_GetPID());
    system_PrintChar('\n');
    //a[0]=1994;
    x = system_Fork();
    if (x == 0) {
      // a[0]=1886;
       system_PrintString("Child PID: ");
       system_PrintInt(system_GetPID());
       system_PrintChar('\n');
       system_PrintString("child var init");
      // system_PrintInt(a[0]);
       system_PrintChar('\n');
       system_PrintString("Child's parent PID: ");
       system_PrintInt(system_GetPPID());
       system_PrintChar('\n');
       sleep_start = system_GetTime();
       system_Sleep(100);
       sleep_end = system_GetTime();
       system_PrintString("Child called sleep at time: ");
       system_PrintInt(sleep_start);
       system_PrintChar('\n');
       system_PrintString("Child returned from sleep at time: ");
       system_PrintInt(sleep_end);
       system_PrintChar('\n');
       system_PrintString("Child executed ");
       system_PrintInt(system_GetNumInstr());
       system_PrintString(" instructions.\n");
       system_PrintString("Child var change");
       //system_PrintInt(a[0]);
       system_PrintChar('\n');
    }
    else {
       system_PrintString("Parent after fork waiting for child: ");
       system_PrintInt(x);
       system_PrintChar('\n');
       //a[0]=1810;
       system_PrintString("Parent var change");
      // system_PrintInt(a[0]);
       system_PrintChar('\n');
       system_Join(x);
       system_PrintString("Parent executed ");
       system_PrintInt(system_GetNumInstr());
       system_PrintString(" instructions.\n");
    }
    return 0;
}
