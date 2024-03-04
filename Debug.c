// PicoC interactive debugger.
#ifndef NO_DEBUGGER

#include "Extern.h"

#define HashBP(P) (((unsigned long)(P)->FileName) ^ (((P)->Line << 16) | ((P)->CharacterPos << 16)))

// Initialize the debugger by clearing the breakpoint table.
void DebugInit(State pc) {
   TableInitTable(&pc->BreakpointTable, pc->BreakpointHashTable, DebugMax, true);
   pc->BreakpointCount = 0;
}

// Free the contents of the breakpoint table.
void DebugCleanup(State pc) {
   for (int Count = 0; Count < pc->BreakpointTable.Size; Count++) {
      for (TableEntry Entry = pc->BreakpointHashTable[Count], NextEntry; Entry != NULL; Entry = NextEntry) {
         NextEntry = Entry->Next;
         HeapFreeMem(pc, Entry);
      }
   }
}

// Search the table for a breakpoint.
static TableEntry DebugTableSearchBreakpoint(ParseState Parser, int *AddAt) {
   State pc = Parser->pc;
   int HashValue = HashBP(Parser)%pc->BreakpointTable.Size;
   for (TableEntry Entry = pc->BreakpointHashTable[HashValue]; Entry != NULL; Entry = Entry->Next) {
      if (Entry->p.b.FileName == Parser->FileName && Entry->p.b.Line == Parser->Line && Entry->p.b.CharacterPos == Parser->CharacterPos)
         return Entry; // Found.
   }
   *AddAt = HashValue; // Didn't find it in the chain.
   return NULL;
}

// Set a breakpoint in the table.
void DebugSetBreakpoint(ParseState Parser) {
   int AddAt;
   TableEntry FoundEntry = DebugTableSearchBreakpoint(Parser, &AddAt);
   State pc = Parser->pc;
   if (FoundEntry == NULL) {
   // Add it to the table.
      TableEntry NewEntry = HeapAllocMem(pc, sizeof *NewEntry);
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
bool DebugClearBreakpoint(ParseState Parser) {
   State pc = Parser->pc;
   int HashValue = HashBP(Parser)%pc->BreakpointTable.Size;
   for (TableEntry *EntryPtr = &pc->BreakpointHashTable[HashValue]; *EntryPtr != NULL; EntryPtr = &(*EntryPtr)->Next) {
      TableEntry DeleteEntry = *EntryPtr;
      if (DeleteEntry->p.b.FileName == Parser->FileName && DeleteEntry->p.b.Line == Parser->Line && DeleteEntry->p.b.CharacterPos == Parser->CharacterPos) {
         *EntryPtr = DeleteEntry->Next;
         HeapFreeMem(pc, DeleteEntry);
         pc->BreakpointCount--;
         return true;
      }
   }
   return false;
}

void DebugStep() {
}

// Before we run a statement, check if there's anything we have to do with the debugger here.
void DebugCheckStatement(ParseState Parser) {
   State pc = Parser->pc;
// Has the user manually pressed break?
   bool DoBreak = pc->DebugManualBreak;
   if (DoBreak) {
      PlatformPrintf(pc->CStdOut, "break\n");
      pc->DebugManualBreak = false;
   }
// Is this a breakpoint location?
   int AddAt;
   if (Parser->pc->BreakpointCount != 0 && DebugTableSearchBreakpoint(Parser, &AddAt) != NULL)
      DoBreak = true;
// Handle a break.
   if (DoBreak) {
      PlatformPrintf(pc->CStdOut, "Handling a break\n");
      PicocParseInteractiveNoStartPrompt(pc, false);
   }
}

#endif // !NO_DEBUGGER.
