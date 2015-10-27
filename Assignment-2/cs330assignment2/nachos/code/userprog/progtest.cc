// progtest.cc 
//	Test routines for demonstrating that Nachos can load
//	a user program and execute it.  
//
//	Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "console.h"
#include "addrspace.h"
#include "synch.h"
#include "syscall.h"
#include "mipssim.h"
#include "machine.h"
extern void ForkStartFunction(int);
extern void system_Exit(int);
//----------------------------------------------------------------------
// StartProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------

void
StartProcess(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    space = new AddrSpace(executable);    
    currentThread->space = space;

    delete executable;			// close file

    space->InitRegisters();		// set the initial register values
    space->RestoreState();		// load page table register

    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

void
StartMyProcess(char *filename,int initpriority)
{
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    if (executable == NULL) {
        printf("Unable to open file %s\n", filename);
        return;
    }
    NachOSThread* child = new NachOSThread("Forked new Thread");
    printf("Reached\n");
    child->space = new AddrSpace (executable);                   // Duplicates the address space
    currentThread->SaveUserState();
    child->priority = child->priority + initpriority;         //base priority + initial priority value
    child->space->InitRegisters();     // set the initial register values
    child->SaveUserState ();
    
    child->ThreadStackAllocate (ForkStartFunction, 0);     // Make it ready for a later context switch
    child->Schedule ();
    currentThread->RestoreUserState();
    delete executable;          // close file    
}

void BatchProcess(char *filename)
{
    OpenFile *batchfile = fileSystem->Open(filename);
    char buff;
    int priority=0;
    int count=0,length=batchfile->Length(),totalcount=0;
    char* myfilename=new char[15];
    //printf("%s",filename);
    while(totalcount<length){
         batchfile->Read(&buff,1);
         //printf("%c",buff);
         totalcount++;
        if(buff!=' ' && count>=0 && buff!='\n'){
            myfilename[count]=buff;
            //printf("%c",buff);
            count++;
        }
        else if(buff==' '){
            myfilename[count]='\0';
            //printf("%s",myfilename);
            count=-1;
        }
	else if (count==-1){
		priority = priority*10+(buff-48);
	}
        else if(buff=='\n' || buff==EOF){
            if(count>=0){
                myfilename[count]='\0';
            }
            StartMyProcess(myfilename,priority);
            //printf("%s",myfilename);
            count=0;            
        }
    }
    //system_Exit(0);
}

// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
// 	Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

//----------------------------------------------------------------------
// ConsoleTest
// 	Test the console by echoing characters typed at the input onto
//	the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------

void 
ConsoleTest (char *in, char *out)
{
    char ch;

    console = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    
    for (;;) {
	readAvail->P();		// wait for character to arrive
	ch = console->GetChar();
	console->PutChar(ch);	// echo it!
	writeDone->P() ;        // wait for write to finish
	if (ch == 'q') return;  // if q, quit
    }
}
