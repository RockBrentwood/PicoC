// picoc hash table module.
// This hash table code is used for both symbol tables and the shared string table.
#include "Extern.h"

// Initialize the shared string system.
void TableInit(State pc) {
   TableInitTable(&pc->StringTable, pc->StringHashTable, StrTabMax, true);
   pc->StrEmpty = TableStrRegister(pc, "");
}

// Hash function for strings.
static unsigned int TableHash(const char *Key, int Len) {
   unsigned int Hash = Len;
   int Offset;
   int Count;
   for (Count = 0, Offset = 8; Count < Len; Count++, Offset += 7) {
      if (Offset > 8*sizeof(unsigned int) - 7)
         Offset -= 8*sizeof(unsigned int) - 6;
      Hash ^= *Key++ << Offset;
   }
   return Hash;
}

// Initialize a table.
void TableInitTable(Table Tbl, TableEntry *HashTable, int Size, bool OnHeap) {
   Tbl->Size = Size;
   Tbl->OnHeap = OnHeap;
   Tbl->HashTable = HashTable;
   memset((void *)HashTable, '\0', Size*sizeof *HashTable);
}

// Check a hash table entry for a key.
static TableEntry TableSearch(Table Tbl, const char *Key, int *AddAt) {
   TableEntry Entry;
   int HashValue = ((unsigned long)Key)%Tbl->Size; // Shared strings have unique addresses so we don't need to hash them.
   for (Entry = Tbl->HashTable[HashValue]; Entry != NULL; Entry = Entry->Next) {
      if (Entry->p.v.Key == Key)
         return Entry; // Found.
   }
   *AddAt = HashValue; // Didn't find it in the chain.
   return NULL;
}

// Set an identifier to a value.
// Returns false if it already exists.
// Key must be a shared string from TableStrRegister().
bool TableSet(State pc, Table Tbl, char *Key, Value Val, const char *DeclFileName, int DeclLine, int DeclColumn) {
   int AddAt;
   TableEntry FoundEntry = TableSearch(Tbl, Key, &AddAt);
   if (FoundEntry == NULL) { // Add it to the table.
      TableEntry NewEntry = VariableAlloc(pc, NULL, sizeof *NewEntry, Tbl->OnHeap);
      NewEntry->DeclFileName = DeclFileName;
      NewEntry->DeclLine = DeclLine;
      NewEntry->DeclColumn = DeclColumn;
      NewEntry->p.v.Key = Key;
      NewEntry->p.v.Val = Val;
      NewEntry->Next = Tbl->HashTable[AddAt];
      Tbl->HashTable[AddAt] = NewEntry;
      return true;
   }
   return false;
}

// Find a value in a table.
// Returns false if not found.
// Key must be a shared string from TableStrRegister().
bool TableGet(Table Tbl, const char *Key, Value *Val, const char **DeclFileName, int *DeclLine, int *DeclColumn) {
   int AddAt;
   TableEntry FoundEntry = TableSearch(Tbl, Key, &AddAt);
   if (FoundEntry == NULL)
      return false;
   *Val = FoundEntry->p.v.Val;
   if (DeclFileName != NULL) {
      *DeclFileName = FoundEntry->DeclFileName;
      *DeclLine = FoundEntry->DeclLine;
      *DeclColumn = FoundEntry->DeclColumn;
   }
   return true;
}

// Remove an entry from the table.
Value TableDelete(State pc, Table Tbl, const char *Key) {
   TableEntry *EntryPtr;
   int HashValue = ((unsigned long)Key)%Tbl->Size; // Shared strings have unique addresses so we don't need to hash them.
   for (EntryPtr = &Tbl->HashTable[HashValue]; *EntryPtr != NULL; EntryPtr = &(*EntryPtr)->Next) {
      if ((*EntryPtr)->p.v.Key == Key) {
         TableEntry DeleteEntry = *EntryPtr;
         Value Val = DeleteEntry->p.v.Val;
         *EntryPtr = DeleteEntry->Next;
         HeapFreeMem(pc, DeleteEntry);
         return Val;
      }
   }
   return NULL;
}

// Check a hash table entry for an identifier.
static TableEntry TableSearchIdentifier(Table Tbl, const char *Key, int Len, int *AddAt) {
   TableEntry Entry;
   int HashValue = TableHash(Key, Len)%Tbl->Size;
   for (Entry = Tbl->HashTable[HashValue]; Entry != NULL; Entry = Entry->Next) {
      if (strncmp(Entry->p.Key, (char *)Key, Len) == 0 && Entry->p.Key[Len] == '\0')
         return Entry; // Found.
   }
   *AddAt = HashValue; // Didn't find it in the chain.
   return NULL;
}

// Set an identifier and return the identifier.
// Share if possible.
static char *TableSetIdentifier(State pc, Table Tbl, const char *Ident, int IdentLen) {
   int AddAt;
   TableEntry FoundEntry = TableSearchIdentifier(Tbl, Ident, IdentLen, &AddAt);
   if (FoundEntry != NULL)
      return FoundEntry->p.Key;
   else { // Add it to the table - we economize by not allocating the whole structure here.
      TableEntry NewEntry = HeapAllocMem(pc, sizeof *NewEntry - sizeof NewEntry->p + IdentLen + 1);
      if (NewEntry == NULL)
         ProgramFailNoParser(pc, "out of memory");
      strncpy((char *)NewEntry->p.Key, (char *)Ident, IdentLen);
      NewEntry->p.Key[IdentLen] = '\0';
      NewEntry->Next = Tbl->HashTable[AddAt];
      Tbl->HashTable[AddAt] = NewEntry;
      return NewEntry->p.Key;
   }
}

// Register a string in the shared string store.
char *TableStrRegister2(State pc, const char *Str, int Len) {
   return TableSetIdentifier(pc, &pc->StringTable, Str, Len);
}

char *TableStrRegister(State pc, const char *Str) {
   return TableStrRegister2(pc, Str, strlen((char *)Str));
}

// Free all the strings.
void TableStrFree(State pc) {
   TableEntry Entry;
   TableEntry NextEntry;
   int Count;
   for (Count = 0; Count < pc->StringTable.Size; Count++) {
      for (Entry = pc->StringTable.HashTable[Count]; Entry != NULL; Entry = NextEntry) {
         NextEntry = Entry->Next;
         HeapFreeMem(pc, Entry);
      }
   }
}
