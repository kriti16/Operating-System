// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"
#include "machine.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

extern void StartProcess (char*);
SemTableEntry SemTable[15];
ConditionTableEntry conditionTable[15];
void
ForkStartFunction (int dummy)
{
   currentThread->Startup();
   machine->Run();
}

static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp;
    unsigned printvalus;	// Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);
    int exitcode;		// Used in syscall_Exit
    unsigned i;
    char buffer[1024];		// Used in syscall_Exec
    int waitpid;		// Used in syscall_Join
    int whichChild;		// Used in syscall_Join
    NachOSThread *child;		// Used by syscall_Fork
    unsigned sleeptime;		// Used by syscall_Sleep

    if ((which == SyscallException) && (type == syscall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if (which == PageFaultException) {	
        int virtAddr = machine->registers[BadVAddrReg]/PageSize;
     //   printf("Hi");
        currentThread->space->DemandPaging(virtAddr); 
//        currentThread->setStatus(BLOCKED);
        currentThread->SortedInsertInWaitQueue (1000+stats->totalTicks);
//	currentThread->setStatus(RUNNINGi);
   }
    else if ((which == SyscallException) && (type == syscall_CondRemove)) {
       int id = machine->ReadRegister(4);
       machine->WriteRegister(2,0);
       if (conditionTable[id].myCond!=NULL)
          delete conditionTable[id].myCond;
       else 
           machine->WriteRegister(2,-1);
       machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg,machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_CondOp)) {
       int id = machine->ReadRegister(4);
       int command = machine->ReadRegister(5);
       int semID = machine->ReadRegister(6);
       //printf("s it ok till here?%d\n",finalval);
       machine->WriteRegister(2,0);
       if (conditionTable[id].myCond!=NULL) {
          if (command == 0 && SemTable[semID].mysem!=NULL) 
             conditionTable[id].myCond->Wait(SemTable[semID].mysem);
          else if (command == 1) 
             conditionTable[id].myCond->Signal();
          else if (command == 2)
              conditionTable[id].myCond->Broadcast();
          else
              machine->WriteRegister(2,-1);
       }
       else
           machine->WriteRegister(2,-1);
       machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg,machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_CondGet)) {   
       int key = machine->ReadRegister(4);
       bool flag =FALSE;
       for (int i=0;i<15;i++) {
           if(conditionTable[i].myCond !=NULL && conditionTable[i].key==key){
               flag = TRUE;
               machine->WriteRegister(2,i);
               break;
           }
       }
       if (!flag){
          for(int i=0;i<15;i++) {
             if (conditionTable[i].myCond == NULL){
                conditionTable[i].key=key;
                conditionTable[i].myCond = new Condition(0);
		//printf("Is the prob here\n");
                machine->WriteRegister(2,i);
		break;
             }
          }
       }
	//printf("OK till here\n");
       machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg,machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_SemCtl)) {
       int id = machine->ReadRegister(4);
       int command = machine->ReadRegister(5);
       int virtAddrVal = machine->ReadRegister(6);
       int finalval;
       machine->ReadMem(virtAddrVal,sizeof(int),&finalval);
       //printf("s it ok till here?%d\n",finalval);
       machine->WriteRegister(2,0);
       if (SemTable[id].mysem!=NULL) {
          if (command == 0 ) 
             delete SemTable[id].mysem;
          else if (command == 1) {
             finalval = SemTable[id].mysem->getVal();
	     machine->WriteMem(virtAddrVal,sizeof(int),finalval);
          }
          else if (command == 2)
              SemTable[id].mysem->setVal(finalval);
          else
              machine->WriteRegister(2,-1);
       }
       else
           machine->WriteRegister(2,-1);
       machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg,machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_SemOp)) {
       int id = machine->ReadRegister(4);
       int change = machine->ReadRegister(5);
       if (change == 1 && SemTable[id].mysem!=NULL) {
          SemTable[id].mysem->V();
       }
       else if (change == -1 && SemTable[id].mysem!=NULL) {
           SemTable[id].mysem->P();
       }
       else {
           printf("Incorrect function pass Sem op\n");
       }
       machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg,machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_SemGet)) {
       int key = machine->ReadRegister(4);
       bool flag =FALSE;
       for (int i=0;i<15;i++) {
           if(SemTable[i].mysem !=NULL && SemTable[i].key==key){
               flag = TRUE;
               machine->WriteRegister(2,i);
               break;
           }
       }
       if (!flag){
          for(int i=0;i<15;i++) {
             if (SemTable[i].mysem == NULL){
                SemTable[i].key=key;
                SemTable[i].mysem = new Semaphore(0,2);
		//printf("Is the prob here\n");
                machine->WriteRegister(2,i);
		break;
             }
          }
       }
	//printf("OK till here\n");
       machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg,machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_ShmAllocate)) {
       int sharedSize = machine->ReadRegister(4);
      //printf("Yollo\n");
       if (sharedSize % PageSize == 0)
	  sharedSize = sharedSize/PageSize;
       else
          sharedSize = sharedSize/PageSize +1;
       int oldPages = currentThread->space->GetNumPages();
       machine->pageTable = currentThread->space->AddShared(sharedSize);
       machine->pageTableSize+=sharedSize;
       machine->WriteRegister(2,oldPages*PageSize);
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Exit)) {
       exitcode = machine->ReadRegister(4);
       printf("[pid %d]: Exit called. Code: %d\n", currentThread->GetPID(), exitcode);
       // We do not wait for the children to finish.
       // The children will continue to run.
       // We will worry about this when and if we implement signals.
       exitThreadArray[currentThread->GetPID()] = true;

       // Find out if all threads have called exit
       for (i=0; i<thread_index; i++) {
          if (!exitThreadArray[i]) break;
       }
       currentThread->Exit(i==thread_index, exitcode);
    }
    else if ((which == SyscallException) && (type == syscall_Exec)) {
       // Copy the executable name into kernel space
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       i = 0;
       while ((*(char*)&memval) != '\0') {
          buffer[i] = (*(char*)&memval);
          i++;
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       buffer[i] = (*(char*)&memval);
       StartProcess(buffer);
    }
    else if ((which == SyscallException) && (type == syscall_Join)) {
       waitpid = machine->ReadRegister(4);
       // Check if this is my child. If not, return -1.
       whichChild = currentThread->CheckIfChild (waitpid);
       if (whichChild == -1) {
          printf("[pid %d] Cannot join with non-existent child [pid %d].\n", currentThread->GetPID(), waitpid);
          machine->WriteRegister(2, -1);
          // Advance program counters.
          machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
          machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
          machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       }
       else {
          exitcode = currentThread->JoinWithChild (whichChild);
          machine->WriteRegister(2, exitcode);
          // Advance program counters.
          machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
          machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
          machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       }
    }
    else if ((which == SyscallException) && (type == syscall_Fork)) {
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
     //  printf("Hey there from fork\n");
       child = new NachOSThread("Forked thread", GET_NICE_FROM_PARENT);
       child->space = new AddrSpace (currentThread->space);  // Duplicates the address space
       child->SaveUserState ();		     		      // Duplicate the register set
       child->ResetReturnValue ();			     // Sets the return register to zero
       child->ThreadStackAllocate (ForkStartFunction, 0);	// Make it ready for a later context switch
       child->Schedule ();
       machine->WriteRegister(2, child->GetPID());		// Return value for parent
      // printf("Fork might be ok\n");
    }
    else if ((which == SyscallException) && (type == syscall_Yield)) {
       currentThread->YieldCPU();
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
             writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
             writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintChar)) {
        writeDone->P() ;        // wait for previous write to finish
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
          writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetReg)) {
       machine->WriteRegister(2, machine->ReadRegister(machine->ReadRegister(4))); // Return value
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPA)) {
       vaddr = machine->ReadRegister(4);
       machine->WriteRegister(2, machine->GetPA(vaddr));  // Return value
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPID)) {
       machine->WriteRegister(2, currentThread->GetPID());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPPID)) {
       machine->WriteRegister(2, currentThread->GetPPID());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Sleep)) {
       sleeptime = machine->ReadRegister(4);
       if (sleeptime == 0) {
          // emulate a yield
          currentThread->YieldCPU();
       }
       else {
          currentThread->SortedInsertInWaitQueue (sleeptime+stats->totalTicks);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Time)) {
       machine->WriteRegister(2, stats->totalTicks);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_NumInstr)) {
       machine->WriteRegister(2, currentThread->GetInstructionCount());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}
