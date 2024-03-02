// picoc interactive debugger.
#ifndef NO_DEBUGGER

#include "Extern.h"

#define BREAKPOINT_HASH(p) (((unsigned long)(p)->FileName) ^ (((p)->Line << 16) | ((p)->CharacterPos << 16)))

// Initialize the debugger by clearing the breakpoint table.
void DebugInit(Picoc *pc) {
   TableInitTable(&pc->BreakpointTable, &pc->BreakpointHashTable[0], BREAKPOINT_TABLE_SIZE, TRUE);
   pc->BreakpointCount = 0;
}

// Free the contents of the breakpoint table.
void DebugCleanup(Picoc *pc) {
   struct TableEntry *Entry;
   struct TableEntry *NextEntry;
   int Count;
   for (Count = 0; Count < pc->BreakpointTable.Size; Count++) {
      for (Entry = pc->BreakpointHashTable[Count]; Entry != NULL; Entry = NextEntry) {
         NextEntry = Entry->Next;
         HeapFreeMem(pc, Entry);
      }
   }
}

// Search the table for a breakpoint.
static struct TableEntry *DebugTableSearchBreakpoint(struct ParseState *Parser, int *AddAt) {
   struct TableEntry *Entry;
   Picoc *pc = Parser->pc;
   int HashValue = BREAKPOINT_HASH(Parser)%pc->BreakpointTable.Size;
   for (Entry = pc->BreakpointHashTable[HashValue]; Entry != NULL; Entry = Entry->Next) {
      if (Entry->p.b.FileName == Parser->FileName && Entry->p.b.Line == Parser->Line && Entry->p.b.CharacterPos == Parser->CharacterPos)
         return Entry; // Found.
   }
   *AddAt = HashValue; // Didn't find it in the chain.
   return NULL;
}

// Set a breakpoint in the table.
void DebugSetBreakpoint(struct ParseState *Parser) {
   int AddAt;
   struct TableEntry *FoundEntry = DebugTableSearchBreakpoint(Parser, &AddAt);
   Picoc *pc = Parser->pc;
   if (FoundEntry == NULL) {
   // Add it to the table.
      struct TableEntry *NewEntry = HeapAllocMem(pc, sizeof(struct TableEntry));
      if (NewEntry == NULL)
         ProgramFailNoParser(pc, "out of memory");
      NewEntry->p.b.FileName = Parser->FileName;
      NewEntry->p.b.Line = Parser->Line;
      NewEntry->p.b.CharacterPos = Parser->CharacterPos;
      NewEntry->Next = pc->BreakpointHashTable[AddAt];
      pc->BreakpointHashTable[AddAt] = NewEntry;
      pc->BreakpointCount++;
   }
}

// Delete a breakpoint from the hash table.
int DebugClearBreakpoint(struct ParseState *Parser) {
   struct TableEntry **EntryPtr;
   Picoc *pc = Parser->pc;
   int HashValue = BREAKPOINT_HASH(Parser)%pc->BreakpointTable.Size;
   for (EntryPtr = &pc->BreakpointHashTable[HashValue]; *EntryPtr != NULL; EntryPtr = &(*EntryPtr)->Next) {
      struct TableEntry *DeleteEntry = *EntryPtr;
      if (DeleteEntry->p.b.FileName == Parser->FileName && DeleteEntry->p.b.Line == Parser->Line && DeleteEntry->p.b.CharacterPos == Parser->CharacterPos) {
         *EntryPtr = DeleteEntry->Next;
         HeapFreeMem(pc, DeleteEntry);
         pc->BreakpointCount--;
         return TRUE;
      }
   }
   return FALSE;
}

// Before we run a statement, check if there's anything we have to do with the debugger here.
void DebugCheckStatement(struct ParseState *Parser) {
   int DoBreak = FALSE;
   int AddAt;
   Picoc *pc = Parser->pc;
// Has the user manually pressed break?
   if (pc->DebugManualBreak) {
      PlatformPrintf(pc->CStdOut, "break\n");
      DoBreak = TRUE;
      pc->DebugManualBreak = FALSE;
   }
// Is this a breakpoint location?
   if (Parser->pc->BreakpointCount != 0 && DebugTableSearchBreakpoint(Parser, &AddAt) != NULL)
      DoBreak = TRUE;
// Handle a break.
   if (DoBreak) {
      PlatformPrintf(pc->CStdOut, "Handling a break\n");
      PicocParseInteractiveNoStartPrompt(pc, FALSE);
   }
}

void DebugStep() {
}

#endif // !NO_DEBUGGER.
