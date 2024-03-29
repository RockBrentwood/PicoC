// PicoC variable storage.
// This provides ways of defining and accessing variables.
#include "Extern.h"

// Initialize the variable system.
void VariableInit(State pc) {
   TableInitTable(&pc->GlobalTable, pc->GlobalHashTable, GloTabMax, true);
   TableInitTable(&pc->StringLiteralTable, pc->StringLiteralHashTable, LitTabMax, true);
   pc->TopStackFrame = NULL;
}

// Deallocate the contents of a variable.
void VariableFree(State pc, Value Val) {
   if (Val->ValOnHeap || Val->AnyValOnHeap) {
   // Free function bodies.
      if (Val->Typ == &pc->FunctionType && Val->Val->FuncDef.Intrinsic == NULL && Val->Val->FuncDef.Body.Pos != NULL)
         HeapFreeMem(pc, (void *)Val->Val->FuncDef.Body.Pos);
   // Free macro bodies.
      if (Val->Typ == &pc->MacroType)
         HeapFreeMem(pc, (void *)Val->Val->MacroDef.Body.Pos);
   // Free the AnyValue.
      if (Val->AnyValOnHeap)
         HeapFreeMem(pc, Val->Val);
   }
// Free the value.
   if (Val->ValOnHeap)
      HeapFreeMem(pc, Val);
}

// Deallocate the global table and the string literal table.
void VariableTableCleanup(State pc, Table HashTable) {
   for (int Count = 0; Count < HashTable->Size; Count++) {
      for (TableEntry Entry = HashTable->HashTable[Count], NextEntry; Entry != NULL; Entry = NextEntry) {
         NextEntry = Entry->Next;
         VariableFree(pc, Entry->p.v.Val);
      // Free the hash table entry.
         HeapFreeMem(pc, Entry);
      }
   }
}

void VariableCleanup(State pc) {
   VariableTableCleanup(pc, &pc->GlobalTable);
   VariableTableCleanup(pc, &pc->StringLiteralTable);
}

// Allocate some memory, either on the heap or the stack and check if we've run out.
void *VariableAlloc(State pc, ParseState Parser, int Size, bool OnHeap) {
   void *NewValue = OnHeap? HeapAllocMem(pc, Size): HeapAllocStack(pc, Size);
   if (NewValue == NULL)
      ProgramFail(Parser, "out of memory");
#ifdef DEBUG_HEAP
   if (!OnHeap)
      printf("pushing %d at 0x%lx\n", Size, (unsigned long)NewValue);
#endif
   return NewValue;
}

// Allocate a value either on the heap or the stack using space dependent on what type we want.
Value VariableAllocValueAndData(State pc, ParseState Parser, int DataSize, bool IsLValue, Value LValueFrom, bool OnHeap) {
   Value NewValue = VariableAlloc(pc, Parser, MemAlign(sizeof *NewValue) + DataSize, OnHeap);
   NewValue->Val = (AnyValue)AddAlign(NewValue, sizeof *NewValue);
   NewValue->ValOnHeap = OnHeap;
   NewValue->AnyValOnHeap = false;
   NewValue->ValOnStack = !OnHeap;
   NewValue->IsLValue = IsLValue;
   NewValue->LValueFrom = LValueFrom;
   if (Parser)
      NewValue->ScopeID = Parser->ScopeID;
   NewValue->OutOfScope = false;
   return NewValue;
}

// Allocate a value given its type.
Value VariableAllocValueFromType(State pc, ParseState Parser, ValueType Typ, bool IsLValue, Value LValueFrom, bool OnHeap) {
   int Size = TypeSize(Typ, Typ->ArraySize, false);
   Value NewValue = VariableAllocValueAndData(pc, Parser, Size, IsLValue, LValueFrom, OnHeap);
   assert(Size >= 0 || Typ == &pc->VoidType);
   NewValue->Typ = Typ;
   return NewValue;
}

// Allocate a value either on the heap or the stack and copy its value.
// Handles overlapping data.
Value VariableAllocValueAndCopy(State pc, ParseState Parser, Value FromValue, bool OnHeap) {
   ValueType DType = FromValue->Typ;
   char TmpBuf[0x100]; const size_t TmpBufMax = sizeof TmpBuf/sizeof TmpBuf[0];
   int CopySize = TypeSizeValue(FromValue, true);
   assert(CopySize <= TmpBufMax);
   memcpy((void *)TmpBuf, (void *)FromValue->Val, CopySize);
   Value NewValue = VariableAllocValueAndData(pc, Parser, CopySize, FromValue->IsLValue, FromValue->LValueFrom, OnHeap);
   NewValue->Typ = DType;
   memcpy((void *)NewValue->Val, (void *)TmpBuf, CopySize);
   return NewValue;
}

// Allocate a value either on the heap or the stack from an existing AnyValue and type.
Value VariableAllocValueFromExistingData(ParseState Parser, ValueType Typ, AnyValue FromValue, bool IsLValue, Value LValueFrom) {
   Value NewValue = VariableAlloc(Parser->pc, Parser, sizeof *NewValue, false);
   NewValue->Typ = Typ;
   NewValue->Val = FromValue;
   NewValue->ValOnHeap = false;
   NewValue->AnyValOnHeap = false;
   NewValue->ValOnStack = false;
   NewValue->IsLValue = IsLValue;
   NewValue->LValueFrom = LValueFrom;
   return NewValue;
}

// Allocate a value either on the heap or the stack from an existing Value, sharing the value.
Value VariableAllocValueShared(ParseState Parser, Value FromValue) {
   return VariableAllocValueFromExistingData(Parser, FromValue->Typ, FromValue->Val, FromValue->IsLValue, FromValue->IsLValue? FromValue: NULL);
}

// Reallocate a variable so its data has a new size.
void VariableRealloc(ParseState Parser, Value FromValue, int NewSize) {
   if (FromValue->AnyValOnHeap)
      HeapFreeMem(Parser->pc, FromValue->Val);
   FromValue->Val = VariableAlloc(Parser->pc, Parser, NewSize, true);
   FromValue->AnyValOnHeap = true;
}

int VariableScopeBegin(ParseState Parser, int *OldScopeID) {
   State pc = Parser->pc;
#ifdef VAR_SCOPE_DEBUG
   bool FirstPrint = false;
#endif
   Table HashTable = pc->TopStackFrame == NULL? &pc->GlobalTable: &pc->TopStackFrame->LocalTable;
   if (Parser->ScopeID == -1) return -1;
// XXX dumb hash, let's hope for no collisions...
   *OldScopeID = Parser->ScopeID;
   Parser->ScopeID = (int)(intptr_t)(Parser->SourceText)*((int)(intptr_t)(Parser->Pos)/sizeof(char *));
#if 0
// Or maybe a more human-readable hash for debugging?
   Parser->ScopeID = Parser->Line*0x10000 + Parser->CharacterPos;
#endif
   for (int Count = 0; Count < HashTable->Size; Count++) {
      for (TableEntry Entry = HashTable->HashTable[Count], NextEntry; Entry != NULL; Entry = NextEntry) {
         NextEntry = Entry->Next;
         if (Entry->p.v.Val->ScopeID == Parser->ScopeID && Entry->p.v.Val->OutOfScope) {
            Entry->p.v.Val->OutOfScope = false;
            Entry->p.v.Key = (char *)((intptr_t)Entry->p.v.Key&~1);
#ifdef VAR_SCOPE_DEBUG
            if (!FirstPrint) {
               ShowSourcePos(Parser);
            }
            FirstPrint = true;
            printf(">>> back into scope: %s %x %d\n", Entry->p.v.Key, Entry->p.v.Val->ScopeID, Entry->p.v.Val->Val->Integer);
#endif
         }
      }
   }
   return Parser->ScopeID;
}

void VariableScopeEnd(ParseState Parser, int ScopeID, int PrevScopeID) {
   State pc = Parser->pc;
#ifdef VAR_SCOPE_DEBUG
   bool FirstPrint = false;
#endif
   Table HashTable = pc->TopStackFrame == NULL? &pc->GlobalTable: &pc->TopStackFrame->LocalTable;
   if (ScopeID == -1) return;
   for (int Count = 0; Count < HashTable->Size; Count++) {
      for (TableEntry Entry = HashTable->HashTable[Count], NextEntry; Entry != NULL; Entry = NextEntry) {
         NextEntry = Entry->Next;
         if (Entry->p.v.Val->ScopeID == ScopeID && !Entry->p.v.Val->OutOfScope) {
#ifdef VAR_SCOPE_DEBUG
            if (!FirstPrint) {
               ShowSourcePos(Parser);
            }
            FirstPrint = true;
            printf(">>> out of scope: %s %x %d\n", Entry->p.v.Key, Entry->p.v.Val->ScopeID, Entry->p.v.Val->Val->Integer);
#endif
            Entry->p.v.Val->OutOfScope = true;
            Entry->p.v.Key = (char *)((intptr_t)Entry->p.v.Key | 1); // Alter the key so it won't be found by normal searches.
         }
      }
   }
   Parser->ScopeID = PrevScopeID;
}

bool VariableDefinedAndOutOfScope(State pc, const char *Ident) {
   Table HashTable = pc->TopStackFrame == NULL? &pc->GlobalTable: &pc->TopStackFrame->LocalTable;
   for (int Count = 0; Count < HashTable->Size; Count++) {
      for (TableEntry Entry = HashTable->HashTable[Count]; Entry != NULL; Entry = Entry->Next) {
         if (Entry->p.v.Val->OutOfScope && (char *)((intptr_t)Entry->p.v.Key&~1) == Ident)
            return true;
      }
   }
   return false;
}

// Define a variable.
// Ident must be registered.
Value VariableDefine(State pc, ParseState Parser, char *Ident, Value InitValue, ValueType Typ, bool MakeWritable) {
   Table currentTable = pc->TopStackFrame == NULL? &pc->GlobalTable: &pc->TopStackFrame->LocalTable;
   int ScopeID = Parser? Parser->ScopeID: -1;
#ifdef VAR_SCOPE_DEBUG
   if (Parser) fprintf(stderr, "def %s %x (%s:%d:%d)\n", Ident, ScopeID, Parser->FileName, Parser->Line, Parser->CharacterPos);
#endif
   Value AssignValue = InitValue != NULL?
      VariableAllocValueAndCopy(pc, Parser, InitValue, pc->TopStackFrame == NULL):
      VariableAllocValueFromType(pc, Parser, Typ, MakeWritable, NULL, pc->TopStackFrame == NULL);
   AssignValue->IsLValue = MakeWritable;
   AssignValue->ScopeID = ScopeID;
   AssignValue->OutOfScope = false;
   if (!TableSet(pc, currentTable, Ident, AssignValue, Parser? (char *)Parser->FileName: NULL, Parser? Parser->Line: 0, Parser? Parser->CharacterPos: 0))
      ProgramFail(Parser, "'%s' is already defined", Ident);
   return AssignValue;
}

// Define a variable.
// Ident must be registered.
// If it's a redefinition from the same declaration don't throw an error.
Value VariableDefineButIgnoreIdentical(ParseState Parser, char *Ident, ValueType Typ, bool IsStatic, bool *FirstVisit) {
   State pc = Parser->pc;
// Is the type a forward declaration?
   if (TypeIsForwardDeclared(Parser, Typ))
      ProgramFail(Parser, "type '%t' isn't defined", Typ);
   if (IsStatic) {
   // Make the mangled static name (avoiding using sprintf() to minimize library impact).
      char MangledName[LineBufMax];
      char *MNPos = MangledName;
      char *MNEnd = &MangledName[LineBufMax - 1];
      memset((void *)&MangledName, '\0', sizeof MangledName);
      *MNPos++ = '/';
      strncpy(MNPos, (char *)Parser->FileName, MNEnd - MNPos);
      MNPos += strlen(MNPos);
      if (pc->TopStackFrame != NULL) {
      // We're inside a function.
         if (MNEnd - MNPos > 0) *MNPos++ = '/';
         strncpy(MNPos, (char *)pc->TopStackFrame->FuncName, MNEnd - MNPos);
         MNPos += strlen(MNPos);
      }
      if (MNEnd - MNPos > 0) *MNPos++ = '/';
      strncpy(MNPos, Ident, MNEnd - MNPos);
      const char *RegisteredMangledName = TableStrRegister(pc, MangledName);
   // Is this static already defined?
      const char *DeclFileName; int DeclLine, DeclColumn;
      Value ExistingValue = TableGet(&pc->GlobalTable, RegisteredMangledName, &DeclFileName, &DeclLine, &DeclColumn);
      if (ExistingValue == NULL) {
      // Define the mangled-named static variable store in the global scope.
         ExistingValue = VariableAllocValueFromType(Parser->pc, Parser, Typ, true, NULL, true);
         TableSet(pc, &pc->GlobalTable, (char *)RegisteredMangledName, ExistingValue, (char *)Parser->FileName, Parser->Line, Parser->CharacterPos);
         *FirstVisit = true;
      }
   // Static variable exists in the global scope - now make a mirroring variable in our own scope with the short name.
      VariableDefinePlatformVar(Parser->pc, Parser, Ident, ExistingValue->Typ, ExistingValue->Val, true);
      return ExistingValue;
   } else if (Parser->Line == 0) return false;
   else {
      const char *DeclFileName; int DeclLine, DeclColumn;
      Value ExistingValue = TableGet(pc->TopStackFrame == NULL? &pc->GlobalTable: &pc->TopStackFrame->LocalTable, Ident, &DeclFileName, &DeclLine, &DeclColumn);
      return
         ExistingValue != NULL && DeclFileName == Parser->FileName && DeclLine == Parser->Line && DeclColumn == Parser->CharacterPos?
         ExistingValue:
         VariableDefine(Parser->pc, Parser, Ident, NULL, Typ, true);
   }
}

// Check if a variable with a given name is defined.
// Ident must be registered.
bool VariableDefined(State pc, const char *Ident) {
   return
      (pc->TopStackFrame != NULL && TableGet(&pc->TopStackFrame->LocalTable, Ident, NULL, NULL, NULL) != NULL) ||
      TableGet(&pc->GlobalTable, Ident, NULL, NULL, NULL) != NULL;
}

// Get the value of a variable.
// Must be defined.
// Ident must be registered.
Value VariableGet(State pc, ParseState Parser, const char *Ident) {
   Value LVal = pc->TopStackFrame == NULL? NULL: TableGet(&pc->TopStackFrame->LocalTable, Ident, NULL, NULL, NULL);
   if (LVal == NULL) {
      if ((LVal = TableGet(&pc->GlobalTable, Ident, NULL, NULL, NULL)) == NULL) {
         ProgramFail(Parser, "'%s' %s", Ident, VariableDefinedAndOutOfScope(pc, Ident)? "is out of scope": "is undefined");
      }
   }
   return LVal;
}

// Define a global variable shared with a platform global.
// Ident will be registered.
void VariableDefinePlatformVar(State pc, ParseState Parser, char *Ident, ValueType Typ, AnyValue FromValue, bool IsWritable) {
   Value SomeValue = VariableAllocValueAndData(pc, NULL, 0, IsWritable, NULL, true);
   SomeValue->Typ = Typ;
   SomeValue->Val = FromValue;
   if (!TableSet(pc, pc->TopStackFrame == NULL? &pc->GlobalTable: &pc->TopStackFrame->LocalTable, TableStrRegister(pc, Ident), SomeValue, Parser? Parser->FileName: NULL, Parser? Parser->Line: 0, Parser? Parser->CharacterPos: 0))
      ProgramFail(Parser, "'%s' is already defined", Ident);
}

// Free and/or pop the top value off the stack.
// Var must be the top value on the stack!
void VariableStackPop(ParseState Parser, Value Var) {
#ifdef DEBUG_HEAP
   if (Var->ValOnStack)
      printf("popping %ld at 0x%lx\n", (unsigned long)(sizeof *Var + TypeSizeValue(Var, false)), (unsigned long)Var);
#endif
   bool OnHeap = Var->ValOnHeap;
   if (OnHeap) {
      if (Var->Val != NULL)
         HeapFreeMem(Parser->pc, Var->Val);
   }
// Free from the heap, if on heap, else free from the stack, if on the stack, otherwise the value isn't our problem.
   bool Success = HeapPopStack(Parser->pc, Var,
      !OnHeap && Var->ValOnStack? sizeof *Var + TypeSizeValue(Var, false): sizeof *Var
   );
   if (!Success)
      ProgramFail(Parser, "stack underrun");
}

// Add a stack frame when doing a function call.
void VariableStackFrameAdd(ParseState Parser, const char *FuncName, int NumParams) {
   HeapPushStackFrame(Parser->pc);
   StackFrame NewFrame = HeapAllocStack(Parser->pc, sizeof *NewFrame + NumParams*sizeof *NewFrame->Parameter);
   if (NewFrame == NULL)
      ProgramFail(Parser, "out of memory");
   ParserCopy(&NewFrame->ReturnParser, Parser);
   NewFrame->FuncName = FuncName;
   NewFrame->Parameter = NumParams > 0? (void *)((char *)NewFrame + sizeof *NewFrame): NULL;
   TableInitTable(&NewFrame->LocalTable, NewFrame->LocalHashTable, LocTabMax, false);
   NewFrame->PreviousStackFrame = Parser->pc->TopStackFrame;
   Parser->pc->TopStackFrame = NewFrame;
}

// Remove a stack frame.
void VariableStackFramePop(ParseState Parser) {
   if (Parser->pc->TopStackFrame == NULL)
      ProgramFail(Parser, "stack is empty - can't go back");
   ParserCopy(Parser, &Parser->pc->TopStackFrame->ReturnParser);
   Parser->pc->TopStackFrame = Parser->pc->TopStackFrame->PreviousStackFrame;
   HeapPopStackFrame(Parser->pc);
}

// Get a string literal.
// Assumes that Ident is already registered.
// NULL if not found.
Value VariableStringLiteralGet(State pc, char *Ident) {
   return TableGet(&pc->StringLiteralTable, Ident, NULL, NULL, NULL);
}

// Define a string literal.
// Assumes that Ident is already registered.
void VariableStringLiteralDefine(State pc, char *Ident, Value Val) {
   TableSet(pc, &pc->StringLiteralTable, Ident, Val, NULL, 0, 0);
}

// Check a pointer for validity and dereference it for use.
void *VariableDereferencePointer(ParseState Parser, Value PointerValue, Value *DerefVal, int *DerefOffset, ValueType *DerefType, bool *DerefIsLValue) {
   if (DerefVal != NULL)
      *DerefVal = NULL;
   if (DerefType != NULL)
      *DerefType = PointerValue->Typ->FromType;
   if (DerefOffset != NULL)
      *DerefOffset = 0;
   if (DerefIsLValue != NULL)
      *DerefIsLValue = true;
   return PointerValue->Val->Pointer;
}
