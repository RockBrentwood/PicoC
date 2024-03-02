// picoc heap memory allocation.
// This is a complete (but small) memory allocator for embedded systems which have no memory allocator.
// Alternatively you can define USE_MALLOC_HEAP to use your system's own malloc() allocator.

// Stack grows up from the bottom and heap grows down from the top of heap space.
#include "Extern.h"

#ifdef DEBUG_HEAP
static void ShowBigList(State pc) {
   AllocNode LPos;
   printf("Heap: bottom=0x%lx 0x%lx-0x%lx, big freelist=", (long)pc->HeapBottom, (long)&(pc->HeapMemory)[0], (long)&(pc->HeapMemory)[HEAP_SIZE]);
   for (LPos = pc->FreeListBig; LPos != NULL; LPos = LPos->NextFree)
      printf("0x%lx:%d ", (long)LPos, LPos->Size);
   printf("\n");
}
#endif

// Initialize the stack and heap storage.
void HeapInit(State pc, int StackOrHeapSize) {
   int Count;
   int AlignOffset = 0;
#ifdef USE_MALLOC_STACK
   pc->HeapMemory = malloc(StackOrHeapSize);
   pc->HeapBottom = NULL; // The bottom of the (downward-growing) heap.
   pc->StackFrame = NULL; // The current stack frame.
   pc->HeapStackTop = NULL; // The top of the stack.
#else
#ifdef SURVEYOR_HOST
   pc->HeapMemory = (unsigned char *)C_HEAPSTART; // All memory - stack and heap.
   pc->HeapBottom = (void *)C_HEAPSTART + HEAP_SIZE; // The bottom of the (downward-growing) heap.
   pc->StackFrame = (void *)C_HEAPSTART; // The current stack frame.
   pc->HeapStackTop = (void *)C_HEAPSTART; // The top of the stack.
   pc->HeapMemStart = (void *)C_HEAPSTART;
#else
   pc->HeapBottom = &HeapMemory[HEAP_SIZE]; // The bottom of the (downward-growing) heap.
   pc->StackFrame = &HeapMemory[0]; // The current stack frame.
   pc->HeapStackTop = &HeapMemory[0]; // The top of the stack.
#endif
#endif
   while (((unsigned long)&pc->HeapMemory[AlignOffset]&(sizeof(ALIGN_TYPE) - 1)) != 0)
      AlignOffset++;
   pc->StackFrame = &(pc->HeapMemory)[AlignOffset];
   pc->HeapStackTop = &(pc->HeapMemory)[AlignOffset];
   *(void **)(pc->StackFrame) = NULL;
   pc->HeapBottom = &(pc->HeapMemory)[StackOrHeapSize - sizeof(ALIGN_TYPE) + AlignOffset];
   pc->FreeListBig = NULL;
   for (Count = 0; Count < FREELIST_BUCKETS; Count++)
      pc->FreeListBucket[Count] = NULL;
}

void HeapCleanup(State pc) {
#ifdef USE_MALLOC_STACK
   free(pc->HeapMemory);
#endif
}

// Allocate some space on the stack, in the current stack frame.
// Clears memory.
// Can return NULL if out of stack space.
void *HeapAllocStack(State pc, int Size) {
   char *NewMem = pc->HeapStackTop;
   char *NewTop = (char *)pc->HeapStackTop + MEM_ALIGN(Size);
#ifdef DEBUG_HEAP
   printf("HeapAllocStack(%ld) at 0x%lx\n", (unsigned long)MEM_ALIGN(Size), (unsigned long)pc->HeapStackTop);
#endif
   if (NewTop > (char *)pc->HeapBottom)
      return NULL;
   pc->HeapStackTop = (void *)NewTop;
   memset((void *)NewMem, '\0', Size);
   return NewMem;
}

// Allocate some space on the stack, in the current stack frame.
void HeapUnpopStack(State pc, int Size) {
#ifdef DEBUG_HEAP
   printf("HeapUnpopStack(%ld) at 0x%lx\n", (unsigned long)MEM_ALIGN(Size), (unsigned long)pc->HeapStackTop);
#endif
   pc->HeapStackTop = (void *)((char *)pc->HeapStackTop + MEM_ALIGN(Size));
}

// Free some space at the top of the stack.
bool HeapPopStack(State pc, void *Addr, int Size) {
   int ToLose = MEM_ALIGN(Size);
   if (ToLose > ((char *)pc->HeapStackTop - (char *)&(pc->HeapMemory)[0]))
      return false;
#ifdef DEBUG_HEAP
   printf("HeapPopStack(0x%lx, %ld) back to 0x%lx\n", (unsigned long)Addr, (unsigned long)MEM_ALIGN(Size), (unsigned long)pc->HeapStackTop - ToLose);
#endif
   pc->HeapStackTop = (void *)((char *)pc->HeapStackTop - ToLose);
   assert(Addr == NULL || pc->HeapStackTop == Addr);
   return true;
}

// Push a new stack frame on to the stack.
void HeapPushStackFrame(State pc) {
#ifdef DEBUG_HEAP
   printf("Adding stack frame at 0x%lx\n", (unsigned long)pc->HeapStackTop);
#endif
   *(void **)pc->HeapStackTop = pc->StackFrame;
   pc->StackFrame = pc->HeapStackTop;
   pc->HeapStackTop = (void *)((char *)pc->HeapStackTop + MEM_ALIGN(sizeof(ALIGN_TYPE)));
}

// Pop the current stack frame, freeing all memory in the frame.
// Can return NULL.
bool HeapPopStackFrame(State pc) {
   if (*(void **)pc->StackFrame != NULL) {
      pc->HeapStackTop = pc->StackFrame;
      pc->StackFrame = *(void **)pc->StackFrame;
#ifdef DEBUG_HEAP
      printf("Popping stack frame back to 0x%lx\n", (unsigned long)pc->HeapStackTop);
#endif
      return true;
   } else
      return false;
}

// Allocate some dynamically allocated memory.
// Memory is cleared.
// Can return NULL if out of memory.
void *HeapAllocMem(State pc, int Size) {
#ifdef USE_MALLOC_HEAP
   return calloc(Size, 1);
#else
   AllocNode NewMem = NULL;
   AllocNode *FreeNode;
   int AllocSize = MEM_ALIGN(Size) + MEM_ALIGN(sizeof(NewMem->Size));
   int Bucket;
   void *ReturnMem;
   if (Size == 0)
      return NULL;
   assert(Size > 0);
// Make sure we have enough space for an AllocNode.
   if (AllocSize < sizeof(struct AllocNode))
      AllocSize = sizeof(struct AllocNode);
   Bucket = AllocSize >> 2;
   if (Bucket < FREELIST_BUCKETS && pc->FreeListBucket[Bucket] != NULL) {
   // Try to allocate from a freelist bucket first.
#   ifdef DEBUG_HEAP
      printf("allocating %d(%d) from bucket", Size, AllocSize);
#   endif
      NewMem = pc->FreeListBucket[Bucket];
      assert((unsigned long)NewMem >= (unsigned long)&(pc->HeapMemory)[0] && (unsigned char *)NewMem - &(pc->HeapMemory)[0] < HEAP_SIZE);
      pc->FreeListBucket[Bucket] = *(AllocNode *)NewMem;
      assert(pc->FreeListBucket[Bucket] == NULL || ((unsigned long)pc->FreeListBucket[Bucket] >= (unsigned long)&(pc->HeapMemory)[0] && (unsigned char *)pc->FreeListBucket[Bucket] - &(pc->HeapMemory)[0] < HEAP_SIZE));
      NewMem->Size = AllocSize;
   } else if (pc->FreeListBig != NULL) {
   // Grab the first item from the "big" freelist we can fit in.
      for (FreeNode = &pc->FreeListBig; *FreeNode != NULL && (*FreeNode)->Size < AllocSize; FreeNode = &(*FreeNode)->NextFree) {
      }
      if (*FreeNode != NULL) {
         assert((unsigned long)*FreeNode >= (unsigned long)&(pc->HeapMemory)[0] && (unsigned char *)*FreeNode - &(pc->HeapMemory)[0] < HEAP_SIZE);
         assert((*FreeNode)->Size < HEAP_SIZE && (*FreeNode)->Size > 0);
         if ((*FreeNode)->Size < AllocSize + SPLIT_MEM_THRESHOLD) {
         // Close in size - reduce fragmentation by not splitting.
#   ifdef DEBUG_HEAP
            printf("allocating %d(%d) from freelist, no split (%d)", Size, AllocSize, (*FreeNode)->Size);
#   endif
            NewMem = *FreeNode;
            assert((unsigned long)NewMem >= (unsigned long)&(pc->HeapMemory)[0] && (unsigned char *)NewMem - &(pc->HeapMemory)[0] < HEAP_SIZE);
            *FreeNode = NewMem->NextFree;
         } else {
         // Split this big memory chunk.
#   ifdef DEBUG_HEAP
            printf("allocating %d(%d) from freelist, split chunk (%d)", Size, AllocSize, (*FreeNode)->Size);
#   endif
            NewMem = (void *)((char *)*FreeNode + (*FreeNode)->Size - AllocSize);
            assert((unsigned long)NewMem >= (unsigned long)&(pc->HeapMemory)[0] && (unsigned char *)NewMem - &(pc->HeapMemory)[0] < HEAP_SIZE);
            (*FreeNode)->Size -= AllocSize;
            NewMem->Size = AllocSize;
         }
      }
   }
   if (NewMem == NULL) {
   // Couldn't allocate from a freelist - try to increase the size of the heap area.
#   ifdef DEBUG_HEAP
      printf("allocating %d(%d) at bottom of heap (0x%lx-0x%lx)", Size, AllocSize, (long)((char *)pc->HeapBottom - AllocSize), (long)HeapBottom);
#   endif
      if ((char *)pc->HeapBottom - AllocSize < (char *)pc->HeapStackTop)
         return NULL;
      pc->HeapBottom = (void *)((char *)pc->HeapBottom - AllocSize);
      NewMem = pc->HeapBottom;
      NewMem->Size = AllocSize;
   }
   ReturnMem = (void *)((char *)NewMem + MEM_ALIGN(sizeof(NewMem->Size)));
   memset(ReturnMem, '\0', AllocSize - MEM_ALIGN(sizeof(NewMem->Size)));
#   ifdef DEBUG_HEAP
   printf(" = %lx\n", (unsigned long)ReturnMem);
#   endif
   return ReturnMem;
#endif
}

// Free some dynamically allocated memory.
void HeapFreeMem(State pc, void *Mem) {
#ifdef USE_MALLOC_HEAP
   free(Mem);
#else
   AllocNode MemNode = (AllocNode)((char *)Mem - MEM_ALIGN(sizeof(MemNode->Size)));
   int Bucket = MemNode->Size >> 2;
#   ifdef DEBUG_HEAP
   printf("HeapFreeMem(0x%lx)\n", (unsigned long)Mem);
#   endif
   assert((unsigned long)Mem >= (unsigned long)&(pc->HeapMemory)[0] && (unsigned char *)Mem - &(pc->HeapMemory)[0] < HEAP_SIZE);
   assert(MemNode->Size < HEAP_SIZE && MemNode->Size > 0);
   if (Mem == NULL)
      return;
   if ((void *)MemNode == pc->HeapBottom) {
   // Pop it off the bottom of the heap, reducing the heap size.
#   ifdef DEBUG_HEAP
      printf("freeing %d from bottom of heap\n", MemNode->Size);
#   endif
      pc->HeapBottom = (void *)((char *)pc->HeapBottom + MemNode->Size);
#   ifdef DEBUG_HEAP
      ShowBigList(pc);
#   endif
   } else if (Bucket < FREELIST_BUCKETS) {
   // We can fit it in a bucket.
#   ifdef DEBUG_HEAP
      printf("freeing %d to bucket\n", MemNode->Size);
#   endif
      assert(pc->FreeListBucket[Bucket] == NULL || ((unsigned long)pc->FreeListBucket[Bucket] >= (unsigned long)&(pc->HeapMemory)[0] && (unsigned char *)FreeListBucket[Bucket] - &HeapMemory[0] < HEAP_SIZE));
      *(AllocNode *)MemNode = pc->FreeListBucket[Bucket];
      pc->FreeListBucket[Bucket] = (AllocNode)MemNode;
   } else {
   // Put it in the big memory freelist.
#   ifdef DEBUG_HEAP
      printf("freeing %lx:%d to freelist\n", (unsigned long)Mem, MemNode->Size);
#   endif
      assert(pc->FreeListBig == NULL || ((unsigned long)pc->FreeListBig >= (unsigned long)&(pc->HeapMemory)[0] && (unsigned char *)pc->FreeListBig - &(pc->HeapMemory)[0] < HEAP_SIZE));
      MemNode->NextFree = pc->FreeListBig;
      FreeListBig = MemNode;
#   ifdef DEBUG_HEAP
      ShowBigList(pc);
#   endif
   }
#endif
}
