// PicoC data type module.
// This manages a tree of data types and has facilities for parsing data types.
#include "Extern.h"

// Some basic types.
static int PointerAlignBytes;
static int IntAlignBytes;

// Add a new type to the set of types we know about.
static ValueType TypeAdd(State pc, ParseState Parser, ValueType ParentType, BaseType Base, int ArraySize, const char *Identifier, int Sizeof, int AlignBytes) {
   ValueType NewType = VariableAlloc(pc, Parser, sizeof *NewType, true);
   NewType->Base = Base;
   NewType->ArraySize = ArraySize;
   NewType->Sizeof = Sizeof;
   NewType->AlignBytes = AlignBytes;
   NewType->Identifier = Identifier;
   NewType->Members = NULL;
   NewType->FromType = ParentType;
   NewType->DerivedTypeList = NULL;
   NewType->OnHeap = true;
   NewType->Next = ParentType->DerivedTypeList;
   ParentType->DerivedTypeList = NewType;
   return NewType;
}

// Given a parent type, get a matching derived type and make one if necessary.
// Identifier should be registered with the shared string table.
ValueType TypeGetMatching(State pc, ParseState Parser, ValueType ParentType, BaseType Base, int ArraySize, const char *Identifier, bool AllowDuplicates) {
   int Sizeof;
   int AlignBytes;
   ValueType ThisType = ParentType->DerivedTypeList;
   while (ThisType != NULL && (ThisType->Base != Base || ThisType->ArraySize != ArraySize || ThisType->Identifier != Identifier))
      ThisType = ThisType->Next;
   if (ThisType != NULL) {
      if (AllowDuplicates)
         return ThisType;
      else
         ProgramFail(Parser, "data type '%s' is already defined", Identifier);
   }
   switch (Base) {
      case PointerT: Sizeof = sizeof(void *), AlignBytes = PointerAlignBytes; break;
      case ArrayT: Sizeof = ArraySize*ParentType->Sizeof, AlignBytes = ParentType->AlignBytes; break;
      case EnumT: Sizeof = sizeof(int), AlignBytes = IntAlignBytes; break;
   // Structs and unions will get bigger when we add members to them.
      default: Sizeof = 0, AlignBytes = 0; break;
   }
   return TypeAdd(pc, Parser, ParentType, Base, ArraySize, Identifier, Sizeof, AlignBytes);
}

// Stack space used by a value.
int TypeStackSizeValue(Value Val) {
   return Val != NULL && Val->ValOnStack? TypeSizeValue(Val, false): 0;
}

// Memory used by a value.
int TypeSizeValue(Value Val, bool Compact) {
   return
      IsIntVal(Val) && !Compact? AlignSize: // Allow some extra room for type extension.
      Val->Typ->Base != ArrayT? Val->Typ->Sizeof:
      Val->Typ->FromType->Sizeof*Val->Typ->ArraySize;
}

// Memory used by a variable given its type and array size.
int TypeSize(ValueType Typ, int ArraySize, bool Compact) {
   return
      IsIntType(Typ) && !Compact? AlignSize: // Allow some extra room for type extension.
      Typ->Base != ArrayT? Typ->Sizeof:
      Typ->FromType->Sizeof*ArraySize;
}

// Add a base type.
static void TypeAddBaseType(State pc, ValueType TypeNode, BaseType Base, int Sizeof, int AlignBytes) {
   TypeNode->Base = Base;
   TypeNode->ArraySize = 0;
   TypeNode->Sizeof = Sizeof;
   TypeNode->AlignBytes = AlignBytes;
   TypeNode->Identifier = pc->StrEmpty;
   TypeNode->Members = NULL;
   TypeNode->FromType = NULL;
   TypeNode->DerivedTypeList = NULL;
   TypeNode->OnHeap = false;
   TypeNode->Next = pc->UberType.DerivedTypeList;
   pc->UberType.DerivedTypeList = TypeNode;
}

// Initialize the type system.
void TypeInit(State pc) {
   struct IntAlign {
      char x;
      int y;
   } ia;
   struct ShortAlign {
      char x;
      short y;
   } sa;
   struct CharAlign {
      char x;
      char y;
   } ca;
   struct LongAlign {
      char x;
      long y;
   } la;
#ifndef NO_FP
   struct DoubleAlign {
      char x;
      double y;
   } da;
#endif
   struct PointerAlign {
      char x;
      void *y;
   } pa;
   IntAlignBytes = (char *)&ia.y - &ia.x;
   PointerAlignBytes = (char *)&pa.y - &pa.x;
   pc->UberType.DerivedTypeList = NULL;
   TypeAddBaseType(pc, &pc->IntType, IntT, sizeof ia.y, IntAlignBytes);
   TypeAddBaseType(pc, &pc->ShortType, ShortIntT, sizeof sa.y, (char *)&sa.y - &sa.x);
   TypeAddBaseType(pc, &pc->CharType, CharT, sizeof ca.y, (char *)&ca.y - &ca.x);
   TypeAddBaseType(pc, &pc->LongType, LongIntT, sizeof la.y, (char *)&la.y - &la.x);
   TypeAddBaseType(pc, &pc->UnsignedIntType, NatT, sizeof(unsigned int), IntAlignBytes);
   TypeAddBaseType(pc, &pc->UnsignedShortType, ShortNatT, sizeof(unsigned short), (char *)&sa.y - &sa.x);
   TypeAddBaseType(pc, &pc->UnsignedLongType, LongNatT, sizeof(unsigned long), (char *)&la.y - &la.x);
   TypeAddBaseType(pc, &pc->UnsignedCharType, ByteT, sizeof(unsigned char), (char *)&ca.y - &ca.x);
   TypeAddBaseType(pc, &pc->VoidType, VoidT, 0, 1);
   TypeAddBaseType(pc, &pc->FunctionType, FunctionT, sizeof ia.y, IntAlignBytes);
   TypeAddBaseType(pc, &pc->MacroType, MacroT, sizeof ia.y, IntAlignBytes);
   TypeAddBaseType(pc, &pc->GotoLabelType, LabelT, 0, 1);
#ifndef NO_FP
   TypeAddBaseType(pc, &pc->FPType, RatT, sizeof da.y, (char *)&da.y - &da.x);
   TypeAddBaseType(pc, &pc->TypeType, TypeT, sizeof da.y, (char *)&da.y - &da.x); // Must be large enough to cast to a double.
#else
   TypeAddBaseType(pc, &pc->TypeType, TypeT, sizeof(ValueType), PointerAlignBytes);
#endif
   pc->CharArrayType = TypeAdd(pc, NULL, &pc->CharType, ArrayT, 0, pc->StrEmpty, sizeof ca.y, (char *)&ca.y - &ca.x);
   pc->CharPtrType = TypeAdd(pc, NULL, &pc->CharType, PointerT, 0, pc->StrEmpty, sizeof pa.y, PointerAlignBytes);
   pc->CharPtrPtrType = TypeAdd(pc, NULL, pc->CharPtrType, PointerT, 0, pc->StrEmpty, sizeof pa.y, PointerAlignBytes);
   pc->VoidPtrType = TypeAdd(pc, NULL, &pc->VoidType, PointerT, 0, pc->StrEmpty, sizeof pa.y, PointerAlignBytes);
}

// Deallocate heap-allocated types.
static void TypeCleanupNode(State pc, ValueType Typ) {
   ValueType SubType;
   ValueType NextSubType;
// Clean up and free all the sub-nodes.
   for (SubType = Typ->DerivedTypeList; SubType != NULL; SubType = NextSubType) {
      NextSubType = SubType->Next;
      TypeCleanupNode(pc, SubType);
      if (SubType->OnHeap) {
      // If it's a struct or union deallocate all the member values.
         if (SubType->Members != NULL) {
            VariableTableCleanup(pc, SubType->Members);
            HeapFreeMem(pc, SubType->Members);
         }
      // Free this node.
         HeapFreeMem(pc, SubType);
      }
   }
}

void TypeCleanup(State pc) {
   TypeCleanupNode(pc, &pc->UberType);
}

// Parse a struct or union declaration.
static void TypeParseStruct(ParseState Parser, ValueType *Typ, bool IsStruct) {
   Value LexValue;
   ValueType MemberType;
   char *MemberIdentifier;
   char *StructIdentifier;
   Value MemberValue;
   Lexical Token;
   int AlignBoundary;
   State pc = Parser->pc;
   Token = LexGetToken(Parser, &LexValue, false);
   if (Token == IdL) {
      LexGetToken(Parser, &LexValue, true);
      StructIdentifier = LexValue->Val->Identifier;
      Token = LexGetToken(Parser, NULL, false);
   } else {
      static char TempNameBuf[7] = "^s0000";
      StructIdentifier = PlatformMakeTempName(pc, TempNameBuf);
   }
   *Typ = TypeGetMatching(pc, Parser, &Parser->pc->UberType, IsStruct? StructT: UnionT, 0, StructIdentifier, true);
   if (Token == LCurlL && (*Typ)->Members != NULL)
      ProgramFail(Parser, "data type '%t' is already defined", *Typ);
   Token = LexGetToken(Parser, NULL, false);
   if (Token != LCurlL) {
   // Use the already defined structure.
#if 0
      if ((*Typ)->Members == NULL)
         ProgramFail(Parser, "structure '%s' isn't defined", LexValue->Val->Identifier);
#endif
      return;
   }
   if (pc->TopStackFrame != NULL)
      ProgramFail(Parser, "struct/union definitions can only be globals");
   LexGetToken(Parser, NULL, true);
   (*Typ)->Members = VariableAlloc(pc, Parser, sizeof *(*Typ)->Members + MemTabMax*sizeof(struct TableEntry), true);
   (*Typ)->Members->HashTable = (TableEntry *)((char *)(*Typ)->Members + sizeof *(*Typ)->Members);
   TableInitTable((*Typ)->Members, (TableEntry *)((char *)(*Typ)->Members + sizeof *(*Typ)->Members), MemTabMax, true);
   do {
      TypeParse(Parser, &MemberType, &MemberIdentifier, NULL);
      if (MemberType == NULL || MemberIdentifier == NULL)
         ProgramFail(Parser, "invalid type in struct");
      MemberValue = VariableAllocValueAndData(pc, Parser, sizeof(int), false, NULL, true);
      MemberValue->Typ = MemberType;
      if (IsStruct) {
      // Allocate this member's location in the struct.
         AlignBoundary = MemberValue->Typ->AlignBytes;
         if (((*Typ)->Sizeof&(AlignBoundary - 1)) != 0)
            (*Typ)->Sizeof += AlignBoundary - ((*Typ)->Sizeof&(AlignBoundary - 1));
         MemberValue->Val->Integer = (*Typ)->Sizeof;
         (*Typ)->Sizeof += TypeSizeValue(MemberValue, true);
      } else {
      // Union members always start at 0, make sure it's big enough to hold the largest member.
         MemberValue->Val->Integer = 0;
         if (MemberValue->Typ->Sizeof > (*Typ)->Sizeof)
            (*Typ)->Sizeof = TypeSizeValue(MemberValue, true);
      }
   // Make sure to align to the size of the largest member's alignment.
      if ((*Typ)->AlignBytes < MemberValue->Typ->AlignBytes)
         (*Typ)->AlignBytes = MemberValue->Typ->AlignBytes;
   // Define it.
      if (!TableSet(pc, (*Typ)->Members, MemberIdentifier, MemberValue, Parser->FileName, Parser->Line, Parser->CharacterPos))
         ProgramFail(Parser, "member '%s' already defined", &MemberIdentifier);
      if (LexGetToken(Parser, NULL, true) != SemiL)
         ProgramFail(Parser, "semicolon expected");
   } while (LexGetToken(Parser, NULL, false) != RCurlL);
// Now align the structure to the size of its largest member's alignment.
   AlignBoundary = (*Typ)->AlignBytes;
   if (((*Typ)->Sizeof&(AlignBoundary - 1)) != 0)
      (*Typ)->Sizeof += AlignBoundary - ((*Typ)->Sizeof&(AlignBoundary - 1));
   LexGetToken(Parser, NULL, true);
}

// Create a system struct which has no user-visible members.
ValueType TypeCreateOpaqueStruct(State pc, ParseState Parser, const char *StructName, int Size) {
   ValueType Typ = TypeGetMatching(pc, Parser, &pc->UberType, StructT, 0, StructName, false);
// Create the (empty) table.
   Typ->Members = VariableAlloc(pc, Parser, sizeof *Typ->Members + MemTabMax*sizeof(struct TableEntry), true);
   Typ->Members->HashTable = (TableEntry *)((char *)Typ->Members + sizeof *Typ->Members);
   TableInitTable(Typ->Members, (TableEntry *)((char *)Typ->Members + sizeof *Typ->Members), MemTabMax, true);
   Typ->Sizeof = Size;
   return Typ;
}

// Parse an enum declaration.
static void TypeParseEnum(ParseState Parser, ValueType *Typ) {
   Value LexValue;
   struct Value InitValue;
   Lexical Token;
   int EnumValue = 0;
   char *EnumIdentifier;
   State pc = Parser->pc;
   Token = LexGetToken(Parser, &LexValue, false);
   if (Token == IdL) {
      LexGetToken(Parser, &LexValue, true);
      EnumIdentifier = LexValue->Val->Identifier;
      Token = LexGetToken(Parser, NULL, false);
   } else {
      static char TempNameBuf[7] = "^e0000";
      EnumIdentifier = PlatformMakeTempName(pc, TempNameBuf);
   }
   TypeGetMatching(pc, Parser, &pc->UberType, EnumT, 0, EnumIdentifier, Token != LCurlL);
   *Typ = &pc->IntType;
   if (Token != LCurlL) {
   // Use the already defined enum.
      if ((*Typ)->Members == NULL)
         ProgramFail(Parser, "enum '%s' isn't defined", EnumIdentifier);
      return;
   }
   if (pc->TopStackFrame != NULL)
      ProgramFail(Parser, "enum definitions can only be globals");
   LexGetToken(Parser, NULL, true);
   (*Typ)->Members = &pc->GlobalTable;
   memset((void *)&InitValue, '\0', sizeof InitValue);
   InitValue.Typ = &pc->IntType;
   InitValue.Val = (AnyValue)&EnumValue;
   do {
      if (LexGetToken(Parser, &LexValue, true) != IdL)
         ProgramFail(Parser, "identifier expected");
      EnumIdentifier = LexValue->Val->Identifier;
      if (LexGetToken(Parser, NULL, false) == EquL) {
         LexGetToken(Parser, NULL, true);
         EnumValue = ExpressionParseInt(Parser);
      }
      VariableDefine(pc, Parser, EnumIdentifier, &InitValue, NULL, false);
      Token = LexGetToken(Parser, NULL, true);
      if (Token != CommaL && Token != RCurlL)
         ProgramFail(Parser, "comma expected");
      EnumValue++;
   } while (Token == CommaL);
}

// Parse a type - just the basic type.
bool TypeParseFront(ParseState Parser, ValueType *Typ, bool *IsStatic) {
   struct ParseState Before;
   Value LexerValue;
   Lexical Token;
   bool Unsigned = false;
   Value VarValue;
   bool StaticQualifier = false;
   State pc = Parser->pc;
   *Typ = NULL;
// Ignore leading type qualifiers.
   ParserCopy(&Before, Parser);
   Token = LexGetToken(Parser, &LexerValue, true);
   while (Token == StaticL || Token == AutoL || Token == RegisterL || Token == ExternL) {
      if (Token == StaticL)
         StaticQualifier = true;
      Token = LexGetToken(Parser, &LexerValue, true);
   }
   if (IsStatic != NULL)
      *IsStatic = StaticQualifier;
// Handle signed/unsigned with no trailing type.
   if (Token == SignedL || Token == UnsignedL) {
      Lexical FollowToken = LexGetToken(Parser, &LexerValue, false);
      Unsigned = Token == UnsignedL;
      if (FollowToken != IntL && FollowToken != LongL && FollowToken != ShortL && FollowToken != CharL) {
         *Typ = Token == UnsignedL? &pc->UnsignedIntType: &pc->IntType;
         return true;
      }
      Token = LexGetToken(Parser, &LexerValue, true);
   }
   switch (Token) {
      case IntL: *Typ = Unsigned? &pc->UnsignedIntType: &pc->IntType; break;
      case ShortL: *Typ = Unsigned? &pc->UnsignedShortType: &pc->ShortType; break;
      case CharL: *Typ = Unsigned? &pc->UnsignedCharType: &pc->CharType; break;
      case LongL: *Typ = Unsigned? &pc->UnsignedLongType: &pc->LongType; break;
#ifndef NO_FP
      case FloatL: case DoubleL: *Typ = &pc->FPType; break;
#endif
      case VoidL: *Typ = &pc->VoidType; break;
      case StructL: case UnionL:
         if (*Typ != NULL)
            ProgramFail(Parser, "bad type declaration");
         TypeParseStruct(Parser, Typ, Token == StructL);
      break;
      case EnumL:
         if (*Typ != NULL)
            ProgramFail(Parser, "bad type declaration");
         TypeParseEnum(Parser, Typ);
      break;
   // We already know it's a typedef-defined type because we got here.
      case IdL: VariableGet(pc, Parser, LexerValue->Val->Identifier, &VarValue), *Typ = VarValue->Val->Typ; break;
      default: ParserCopy(Parser, &Before); return false;
   }
   return true;
}

// Parse a type - the part at the end after the identifier. e.g. array specifications etc.
static ValueType TypeParseBack(ParseState Parser, ValueType FromType) {
   Lexical Token;
   struct ParseState Before;
   ParserCopy(&Before, Parser);
   Token = LexGetToken(Parser, NULL, true);
   if (Token == LBrL) {
   // Add another array bound.
      if (LexGetToken(Parser, NULL, false) == RBrL) {
      // An unsized array.
         LexGetToken(Parser, NULL, true);
         return TypeGetMatching(Parser->pc, Parser, TypeParseBack(Parser, FromType), ArrayT, 0, Parser->pc->StrEmpty, true);
      } else {
      // Get a numeric array size.
         RunMode OldMode = Parser->Mode;
         int ArraySize;
         Parser->Mode = RunM;
         ArraySize = ExpressionParseInt(Parser);
         Parser->Mode = OldMode;
         if (LexGetToken(Parser, NULL, true) != RBrL)
            ProgramFail(Parser, "']' expected");
         return TypeGetMatching(Parser->pc, Parser, TypeParseBack(Parser, FromType), ArrayT, ArraySize, Parser->pc->StrEmpty, true);
      }
   } else {
   // The type specification has finished.
      ParserCopy(Parser, &Before);
      return FromType;
   }
}

// Parse a type - the part which is repeated with each identifier in a declaration list.
void TypeParseIdentPart(ParseState Parser, ValueType BasicTyp, ValueType *Typ, char **Identifier) {
   struct ParseState Before;
   Lexical Token;
   Value LexValue;
   bool Done = false;
   *Typ = BasicTyp;
   *Identifier = Parser->pc->StrEmpty;
   while (!Done) {
      ParserCopy(&Before, Parser);
      Token = LexGetToken(Parser, &LexValue, true);
      switch (Token) {
         case LParL:
            if (*Typ != NULL)
               ProgramFail(Parser, "bad type declaration");
            TypeParse(Parser, Typ, Identifier, NULL);
            if (LexGetToken(Parser, NULL, true) != RParL)
               ProgramFail(Parser, "')' expected");
         break;
         case StarL:
            if (*Typ == NULL)
               ProgramFail(Parser, "bad type declaration");
            *Typ = TypeGetMatching(Parser->pc, Parser, *Typ, PointerT, 0, Parser->pc->StrEmpty, true);
         break;
         case IdL:
            if (*Typ == NULL || *Identifier != Parser->pc->StrEmpty)
               ProgramFail(Parser, "bad type declaration");
            *Identifier = LexValue->Val->Identifier;
            Done = true;
         break;
         default: ParserCopy(Parser, &Before), Done = true; break;
      }
   }
   if (*Typ == NULL)
      ProgramFail(Parser, "bad type declaration");
   if (*Identifier != Parser->pc->StrEmpty) {
   // Parse stuff after the identifier.
      *Typ = TypeParseBack(Parser, *Typ);
   }
}

// Parse a type - a complete declaration including identifier.
void TypeParse(ParseState Parser, ValueType *Typ, char **Identifier, bool *IsStatic) {
   ValueType BasicType;
   TypeParseFront(Parser, &BasicType, IsStatic);
   TypeParseIdentPart(Parser, BasicType, Typ, Identifier);
}

// Check if a type has been fully defined - otherwise it's just a forward declaration.
bool TypeIsForwardDeclared(ParseState Parser, ValueType Typ) {
   return Typ->Base == ArrayT?
      TypeIsForwardDeclared(Parser, Typ->FromType):
      (Typ->Base == StructT || Typ->Base == UnionT) && Typ->Members == NULL;
}
