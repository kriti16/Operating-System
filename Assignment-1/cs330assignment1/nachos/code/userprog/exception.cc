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
#include "thread.h"
#include "list.h"
#include "addrspace.h"
#include "progtest.h"

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

void fkernel(int arg = 0)
{
  if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
  threadToBeDestroyed = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {   // if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
        currentThread->space->RestoreState();
    }
#endif
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
    unsigned printvalus;        // Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == syscall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
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
	writeDone->P() ;
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
    else if ((which == SyscallException) && (type == syscall_GetReg)) {
       int mytmp=machine->ReadRegister(4);
       int myval=machine->ReadRegister(mytmp);
	machine->WriteRegister(2,myval);
       //Advance program counters
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if((which == SyscallException) && (type == syscall_GetPA)){
        //printf("Second syscall implementation\n");
        int virtAddr = machine->ReadRegister(4);
        // calculate the virtual page number from the virtual address
        unsigned int vpn = (unsigned) virtAddr / PageSize;
        int entry;
        if (vpn >= machine->pageTableSize) {
            printf("virtual page # %d too large for page table size %d!\n", virtAddr, machine->pageTableSize);
            machine->WriteRegister(2, -1);
        } 
        else if (!machine->pageTable[vpn].valid) {
            printf("virtual page # %d too large for page table size %d!\n", virtAddr, machine->pageTableSize);
            machine->WriteRegister(2, -1);
        }
        else
        { 
                entry =  machine->pageTable[vpn].physicalPage;
                if(entry > NumPhysPages){
                        printf("Phys page # %d too large than NumPhysPages %d!\n", entry, NumPhysPages);
                        machine->WriteRegister(2, -1);
                }
                machine->WriteRegister(2, entry);
        }

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);          
    }
    else if ((which == SyscallException) && (type == syscall_GetPID)) {
       machine->WriteRegister(2,currentThread->getPID());
       //Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Time)){
      Statistics timeObj = *stats;
      machine->WriteRegister(2,timeObj.totalTicks);
      // Advance program counters.
      machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
      machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
      machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPPID)) {
       machine->WriteRegister(2,currentThread->getPPID());
       //Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_NumInstr)){
      Statistics instrObj = *stats;
      machine->WriteRegister(2,instrObj.userTicks);
      // Advance program counters.
      machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
      machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
      machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Yield)) {
       currentThread->YieldCPU();
       //Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Sleep)) {
       int ticks = machine->ReadRegister(4);
       if(ticks==0){
       		currentThread->YieldCPU();
       }
       else{
		int wakeTime = ticks + stats->totalTicks; 
		mySleepList.SortedInsert(currentThread, wakeTime);
		interrupt->SetLevel(IntOff);
		currentThread->PutThreadToSleep();
		interrupt->Enable();
       }
       //Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Exec)) {
       char* execname;
       execname = new char[15];
       int i=0;
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval)!='\0') {
          execname[i] = *(char*)&memval;
          i++;
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       StartProcess(execname);
   }
    else if ((which == SyscallException) && (type == syscall_Fork)) {
       NachOSThread* newThread = new NachOSThread ("Child");

       int S= machine->pageTable[0].physicalPage*PageSize;
       int R= currentThread->space->getNumPages() * PageSize;
       for(int i=0;i<R;i++){
        machine->mainMemory[MainMemoryPage*PageSize+i]=machine->mainMemory[S+i];
       }
       MainMemoryPage+=(R/PageSize);
       printf("MMP:%d\n",MainMemoryPage);
       newThread->SaveUserState();
       newThread->userRegisters[2]=0;
       newThread->userRegisters[PrevPCReg]=newThread->userRegisters[PCReg];
       newThread->userRegisters[PCReg]=newThread->userRegisters[NextPCReg];
       newThread->userRegisters[NextPCReg]+=4;
 
       machine->WriteRegister(2, newThread->getPID());
       AddrSpace* a=new AddrSpace(currentThread->space);
       newThread->space=a;
       newThread->callThreadStackAllocate(fkernel,0);
       interrupt->SetLevel(IntOff);
       scheduler->ReadyToRun(newThread); 
       interrupt->Enable();
       
       //Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
     }
    else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}
