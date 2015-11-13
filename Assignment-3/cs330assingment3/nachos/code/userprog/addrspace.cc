// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    currExecutable=executable;
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages+numPagesAllocated <= NumPhysPages);		// check we're not trying
										// to run anything too big --
										// at least until we have
										// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;
	pageTable[i].physicalPage = i;
	pageTable[i].valid = FALSE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
        pageTable[i].shared = FALSE;     // this memory is not shared
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
   bzero(&machine->mainMemory[numPagesAllocated*PageSize], size);
 
   // numPagesAllocated += numPages;

// then, copy in the code and data segments into memory
   /* if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        vpn = noffH.code.virtualAddr/PageSize;
        offset = noffH.code.virtualAddr%PageSize;
        entry = &pageTable[vpn];
        pageFrame = entry->physicalPage;
        executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
			noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        vpn = noffH.initData.virtualAddr/PageSize;
        offset = noffH.initData.virtualAddr%PageSize;
        entry = &pageTable[vpn];
        pageFrame = entry->physicalPage;
        executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
			noffH.initData.size, noffH.initData.inFileAddr);
    }*/

}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace (AddrSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

AddrSpace::AddrSpace(AddrSpace *parentSpace)
{
    numPages = parentSpace->GetNumPages();
    unsigned i, size = numPages * PageSize;

    ASSERT(numPages+numPagesAllocated <= NumPhysPages);                // check we're not trying
                                                                                // to run anything too big --
                                                                                // at least until we have
	currExecutable = parentSpace->currExecutable;
                                                                                // virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
                                        numPages, size);
    // first, set up the translation
    TranslationEntry* parentPageTable = parentSpace->GetPageTable();
    pageTable = new TranslationEntry[numPages];
    unsigned startAddrChild = numPagesAllocated*PageSize;
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        if (parentPageTable[i].shared){ 
       	      pageFault++;
           pageTable[i].physicalPage = parentPageTable[i].physicalPage;
	}
        else if(parentPageTable[i].valid) {
	IntStatus oldLevel1 = interrupt->SetLevel(IntOff);
           for(int j=0;j<NumPhysPages;j++){
		
		if(!MainMachinePageTable[j]) {
           		pageTable[i].physicalPage = j;
		//printf("%d:%d\n",i,pageTable[i].physicalPage);
			MainMachinePageTable[j] = TRUE;
       	      		pageFault++;
			break;
		}
	   }
 
	(void) interrupt->SetLevel(oldLevel1);
        }
	else{
		pageTable[i].physicalPage = i;
	}
        pageTable[i].valid = parentPageTable[i].valid;
        pageTable[i].use = parentPageTable[i].use;
        pageTable[i].dirty = parentPageTable[i].dirty;
        pageTable[i].shared = parentPageTable[i].shared;
        pageTable[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
    }

    // Copy the contents
    unsigned startAddrParent = parentPageTable[0].physicalPage*PageSize;
    //unsigned startAddrChild = numPagesAllocated*PageSize;
    /*for (i=0; i<size; i++) {
         machine->mainMemory[startAddrChild+i]=machine->mainMemory[startAddrParent+i];
    }*/
    for (i=0; i<numPages; i++) {
       if(!pageTable[i].shared && pageTable[i].valid){
    	bzero(&machine->mainMemory[pageTable[i].physicalPage*PageSize], PageSize);
	//printf("%d:%d & %d:%d\n",i,pageTable[i].physicalPage,i,parentPageTable[i].physicalPage);
          for(int j=0;j<PageSize;j++){
	      machine->mainMemory[j+pageTable[i].physicalPage*PageSize] = machine->mainMemory[j+parentPageTable[i].physicalPage*PageSize];
          }   
       }
    }
//	printf("Hi\n");
    //numPagesAllocated += numPages;
}

//---------------------------------------------------------------------
// AddrSpace::AddShared( SharedSize)
//       Allocate shared pages
//---------------------------------------------------------------------
TranslationEntry*
AddrSpace::AddShared(int sharedSize) 
{
   TranslationEntry * newPageTable = new TranslationEntry [ numPages + sharedSize ];
   for (int i=0;i<numPages;i++) {
       newPageTable[i].virtualPage = pageTable[i].virtualPage;
       newPageTable[i].physicalPage = pageTable[i].physicalPage;
       newPageTable[i].valid = pageTable[i].valid;
       newPageTable[i].use = pageTable[i].use;
       newPageTable[i].dirty = pageTable[i].dirty;
       newPageTable[i].shared = pageTable[i].shared;
       newPageTable[i].readOnly = pageTable[i].readOnly;
   }
   int j=0;
   for (int i=numPages;i < numPages + sharedSize ;i++) {
	IntStatus oldLevel1 = interrupt->SetLevel(IntOff);
       	for(;j<NumPhysPages;j++) 
		if(!MainMachinePageTable[j])
			break;
	newPageTable[i].virtualPage = i;
       	newPageTable[i].physicalPage = j;
        newPageTable[i].valid = TRUE;
	MainMachinePageTable[j] = TRUE;
	(void) interrupt->SetLevel(oldLevel1);
	pageFault++;
        newPageTable[i].use = FALSE;
        newPageTable[i].dirty = FALSE;
        newPageTable[i].shared = TRUE;
        newPageTable[i].readOnly = FALSE;
   }
   //numPagesAllocated +=sharedSize;
   TranslationEntry* tmp = pageTable;
   pageTable = newPageTable;
   delete tmp;
   numPages +=sharedSize;
   return pageTable;
}


//---------------------------------------------i-------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

unsigned
AddrSpace::GetNumPages()
{
   return numPages;
}

TranslationEntry*
AddrSpace::GetPageTable()
{
   return pageTable;

}

void
AddrSpace::DemandPaging(int virtAddr)
{
	if(currExecutable==NULL);
//		printf("virtpage :%d\n",virtAddr);
	
	IntStatus oldLevel2 = interrupt->SetLevel(IntOff);
	for(int i=0;i<NumPhysPages;i++){
		if(!MainMachinePageTable[i]){
			//printf("Physical page:%d\n",i);
			pageTable[virtAddr].physicalPage = i;
			pageTable[virtAddr].valid = TRUE;
			MainMachinePageTable[i] = TRUE;
			NoffHeader noffH;
			currExecutable->ReadAt((char *)&noffH,sizeof(noffH),0);			
    			bzero(&machine->mainMemory[i*PageSize], PageSize);
			currExecutable->ReadAt(&(machine->mainMemory[i*PageSize]),PageSize, noffH.code.inFileAddr + virtAddr*PageSize);
			pageFault++;
			break;
		}
	}
	(void) interrupt->SetLevel(oldLevel2);
}
