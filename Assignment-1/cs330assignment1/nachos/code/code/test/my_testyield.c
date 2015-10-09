#include "syscall.h"

int
main()
{
    int x,y,i,k;

    x = system_Fork();

    for (i=0; i<5; i++) {
       if(i ==0){
         y=system_Fork();
         system_Sleep(100000);
       }
        //system_Yield();
       system_PrintString("*** thread ");
       if(system_GetPID()==1)
        system_Sleep(10000); 
       system_PrintInt(system_GetPID());
       system_PrintString(" looped ");
       system_PrintInt(i);
       system_PrintString(" times.\n");
       system_Yield();

       
    }
    if (x != 0) {
       //system_PrintInt(x);
       system_PrintString("Before join.\n");
       system_Join(x);
       system_PrintString("After join.\n");
    }
    return 0;
}
