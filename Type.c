// picoc data type module.
// This manages a tree of data types and has facilities for parsing data types.
#include "Extern.h"

// Some basic types.
static int PointerAlignBytes;
static int IntAlignBytes;

// Add a new type to the set of types we know about.
ValueType TypeAdd(State pc, ParseState Parser, ValueType ParentType, BaseType Base, int ArraySize, const char *Identifier, int Sizeof, int AlignBytes) {
   ValueType NewType = VariableAlloc(pc, Parser, sizeof(struct ValueType), true);
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
      case TypePointer:
         Sizeof = sizeof(void *);
         AlignBytes = PointerAlignBytes;
      break;
      case TypeArray:
         Sizeof = ArraySize*ParentType->Sizeof;
         AlignBytes = ParentType->AlignBytes;
      break;
      case TypeEnum:
         Sizeof = sizeof(int);
         AlignBytes = IntAlignBytes;
      break;
      default:
         Sizeof = 0;
         AlignBytes = 0;
      break; // Structs and unions will get bigger when we add members to them.
   }
   return TypeAdd(pc, Parser, ParentType, Base, ArraySize, Identifier, Sizeof, AlignBytes);
}

// Stack space used by a value.
int TypeStackSizeValue(Value Val) {
   if (Val != NULL && Val->ValOnStack)
      return TypeSizeValue(Val, false);
   else
      return 0;
}

// Memory used by a value.
int TypeSizeValue(Value Val, bool Compact) {
   if (IS_INTEGER_NUMERIC(Val) && !Compact)
      return sizeof(ALIGN_TYPE); // Allow some extra room for type extension.
   else if (Val->Typ->Base != TypeArray)
      return Val->Typ->Sizeof;
   else
      return Val->Typ->FromType->Sizeof*Val->Typ->ArraySize;
}

// Memory used by a variable given its type and array size.
int TypeSize(ValueType Typ, int ArraySize, bool Compact) {
   if (IS_INTEGER_NUMERIC_TYPE(Typ) && !Compact)
      return sizeof(ALIGN_TYPE); // Allow some extra room for type extension.
   else if (Typ->Base != TypeArray)
      return Typ->Sizeof;
   else
      return Typ->FromType->Sizeof*ArraySize;
}

// Add a base type.
void TypeAddBaseType(State pc, ValueType TypeNode, BaseType Base, int Sizeof, int AlignBytes) {
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
   TypeAddBaseType(pc, &pc->IntType, TypeInt, sizeof(int), IntAlignBytes);
   TypeAddBaseType(pc, &pc->ShortType, TypeShort, sizeof(short), (char *)&sa.y - &sa.x);
   TypeAddBaseType(pc, &pc->CharType, TypeChar, sizeof(char), (char *)&ca.y - &ca.x);
   TypeAddBaseType(pc, &pc->LongType, TypeLong, sizeof(long), (char *)&la.y - &la.x);
   TypeAddBaseType(pc, &pc->UnsignedIntType, TypeUnsignedInt, sizeof(unsigned int), IntAlignBytes);
   TypeAddBaseType(pc, &pc->UnsignedShortType, TypeUnsignedShort, sizeof(unsigned short), (char *)&sa.y - &sa.x);
   TypeAddBaseType(pc, &pc->UnsignedLongType, TypeUnsignedLong, sizeof(unsigned long), (char *)&la.y - &la.x);
   TypeAddBaseType(pc, &pc->UnsignedCharType, TypeUnsignedChar, sizeof(unsigned char), (char *)&ca.y - &ca.x);
   TypeAddBaseType(pc, &pc->VoidType, TypeVoid, 0, 1);
   TypeAddBaseType(pc, &pc->FunctionType, TypeFunction, sizeof(int), IntAlignBytes);
   TypeAddBaseType(pc, &pc->MacroType, TypeMacro, sizeof(int), IntAlignBytes);
   TypeAddBaseType(pc, &pc->GotoLabelType, TypeGotoLabel, 0, 1);
#ifndef NO_FP
   TypeAddBaseType(pc, &pc->FPType, TypeFP, sizeof(double), (char *)&da.y - &da.x);
   TypeAddBaseType(pc, &pc->TypeType, Type_Type, sizeof(double), (char *)&da.y - &da.x); // Must be large enough to cast to a double.
#else
   TypeAddBaseType(pc, &pc->TypeType, Type_Type, sizeof(ValueType), PointerAlignBytes);
#endif
   pc->CharArrayType = TypeAdd(pc, NULL, &pc->CharType, TypeArray, 0, pc->StrEmpty, sizeof(char), (char *)&ca.y - &ca.x);
   pc->CharPtrType = TypeAdd(pc, NULL, &pc->CharType, TypePointer, 0, pc->StrEmpty, sizeof(void *), PointerAlignBytes);
   pc->CharPtrPtrType = TypeAdd(pc, NULL, pc->CharPtrType, TypePointer, 0, pc->StrEmpty, sizeof(void *), PointerAlignBytes);
   pc->VoidPtrType = TypeAdd(pc, NULL, &pc->VoidType, TypePointer, 0, pc->StrEmpty, sizeof(void *), PointerAlignBytes);
}

// Deallocate heap-allocated types.
void TypeCleanupNode(State pc, ValueType Typ) {
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
void TypeParseStruct(ParseState Parser, ValueType *Typ, bool IsStruct) {
   Value LexValue;
   ValueType MemberType;
   char *MemberIdentifier;
   char *StructIdentifier;
   Value MemberValue;
   LexToken Token;
   int AlignBoundary;
   State pc = Parser->pc;
   Token = LexGetToken(Parser, &LexValue, false);
   if (Token == TokenIdentifier) {
      LexGetToken(Parser, &LexValue, true);
      StructIdentifier = LexValue->Val->Identifier;
      Token = LexGetToken(Parser, NULL, false);
   } else {
      static char TempNameBuf[7] = "^s0000";
      StructIdentifier = PlatformMakeTempName(pc, TempNameBuf);
   }
   *Typ = TypeGetMatching(pc, Parser, &Parser->pc->UberType, IsStruct? TypeStruct: TypeUnion, 0, StructIdentifier, true);
   if (Token == TokenLeftBrace && (*Typ)->Members != NULL)
      ProgramFail(Parser, "data type '%t' is already defined", *Typ);
   Token = LexGetToken(Parser, NULL, false);
   if (Token != TokenLeftBrace) {
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
   (*Typ)->Members = VariableAlloc(pc, Parser, sizeof(struct Table) + STRUCT_TABLE_SIZE*sizeof(struct TableEntry), true);
   (*Typ)->Members->HashTable = (TableEntry *)((char *)(*Typ)->Members + sizeof(struct Table));
   TableInitTable((*Typ)->Members, (TableEntry *)((char *)(*Typ)->Members + sizeof(struct Table)), STRUCT_TABLE_SIZE, true);
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
      if (LexGetToken(Parser, NULL, true) != TokenSemicolon)
         ProgramFail(Parser, "semicolon expected");
   } while (LexGetToken(Parser, NULL, false) != TokenRightBrace);
// Now align the structure to the size of its largest member's alignment.
   AlignBoundary = (*Typ)->AlignBytes;
   if (((*Typ)->Sizeof&(AlignBoundary - 1)) != 0)
      (*Typ)->Sizeof += AlignBoundary - ((*Typ)->Sizeof&(AlignBoundary - 1));
   LexGetToken(Parser, NULL, true);
}

// Create a system struct which has no user-visible members.
ValueType TypeCreateOpaqueStruct(State pc, ParseState Parser, const char *StructName, int Size) {
   ValueType Typ = TypeGetMatching(pc, Parser, &pc->UberType, TypeStruct, 0, StructName, false);
// Create the (empty) table.
   Typ->Members = VariableAlloc(pc, Parser, sizeof(struct Table) + STRUCT_TABLE_SIZE*sizeof(struct TableEntry), true);
   Typ->Members->HashTable = (TableEntry *)((char *)Typ->Members + sizeof(struct Table));
   TableInitTable(Typ->Members, (TableEntry *)((char *)Typ->Members + sizeof(struct Table)), STRUCT_TABLE_SIZE, true);
   Typ->Sizeof = Size;
   return Typ;
}

// Parse an enum declaration.
void TypeParseEnum(ParseState Parser, ValueType *Typ) {
   Value LexValue;
   struct Value InitValue;
   LexToken Token;
   int EnumValue = 0;
   char *EnumIdentifier;
   State pc = Parser->pc;
   Token = LexGetToken(Parser, &LexValue, false);
   if (Token == TokenIdentifier) {
      LexGetToken(Parser, &LexValue, true);
      EnumIdentifier = LexValue->Val->Identifier;
      Token = LexGetToken(Parser, NULL, false);
   } else {
      static char TempNameBuf[7] = "^e0000";
      EnumIdentifier = PlatformMakeTempName(pc, TempNameBuf);
   }
   TypeGetMatching(pc, Parser, &pc->UberType, TypeEnum, 0, EnumIdentifier, Token != TokenLeftBrace);
   *Typ = &pc->IntType;
   if (Token != TokenLeftBrace) {
   // Use the already defined enum.
      if ((*Typ)->Members == NULL)
         ProgramFail(Parser, "enum '%s' isn't defined", EnumIdentifier);
      return;
   }
   if (pc->TopStackFrame != NULL)
      ProgramFail(Parser, "enum definitions can only be globals");
   LexGetToken(Parser, NULL, true);
   (*Typ)->Members = &pc->GlobalTable;
   memset((void *)&InitValue, '\0', sizeof(struct Value));
   InitValue.Typ = &pc->IntType;
   InitValue.Val = (AnyValue)&EnumValue;
   do {
      if (LexGetToken(Parser, &LexValue, true) != TokenIdentifier)
         ProgramFail(Parser, "identifier expected");
      EnumIdentifier = LexValue->Val->Identifier;
      if (LexGetToken(Parser, NULL, false) == TokenAssign) {
         LexGetToken(Parser, NULL, true);
         EnumValue = ExpressionParseInt(Parser);
      }
      VariableDefine(pc, Parser, EnumIdentifier, &InitValue, NULL, false);
      Token = LexGetToken(Parser, NULL, true);
      if (Token != TokenComma && Token != TokenRightBrace)
         ProgramFail(Parser, "comma expected");
      EnumValue++;
   } while (Token == TokenComma);
}

// Parse a type - just the basic type.
bool TypeParseFront(ParseState Parser, ValueType *Typ, bool *IsStatic) {
   struct ParseState Before;
   Value LexerValue;
   LexToken Token;
   bool Unsigned = false;
   Value VarValue;
   bool StaticQualifier = false;
   State pc = Parser->pc;
   *Typ = NULL;
// Ignore leading type qualifiers.
   ParserCopy(&Before, Parser);
   Token = LexGetToken(Parser, &LexerValue, true);
   while (Token == TokenStaticType || Token == TokenAutoType || Token == TokenRegisterType || Token == TokenExternType) {
      if (Token == TokenStaticType)
         StaticQualifier = true;
      Token = LexGetToken(Parser, &LexerValue, true);
   }
   if (IsStatic != NULL)
      *IsStatic = StaticQualifier;
// Handle signed/unsigned with no trailing type.
   if (Token == TokenSignedType || Token == TokenUnsignedType) {
      LexToken FollowToken = LexGetToken(Parser, &LexerValue, false);
      Unsigned = (Token == TokenUnsignedType);
      if (FollowToken != TokenIntType && FollowToken != TokenLongType && FollowToken != TokenShortType && FollowToken != TokenCharType) {
         if (Token == TokenUnsignedType)
            *Typ = &pc->UnsignedIntType;
         else
            *Typ = &pc->IntType;
         return true;
      }
      Token = LexGetToken(Parser, &LexerValue, true);
   }
   switch (Token) {
      case TokenIntType:
         *Typ = Unsigned? &pc->UnsignedIntType: &pc->IntType;
      break;
      case TokenShortType:
         *Typ = Unsigned? &pc->UnsignedShortType: &pc->ShortType;
      break;
      case TokenCharType:
         *Typ = Unsigned? &pc->UnsignedCharType: &pc->CharType;
      break;
      case TokenLongType:
         *Typ = Unsigned? &pc->UnsignedLongType: &pc->LongType;
      break;
#ifndef NO_FP
      case TokenFloatType:
      case TokenDoubleType:
         *Typ = &pc->FPType;
      break;
#endif
      case TokenVoidType:
         *Typ = &pc->VoidType;
      break;
      case TokenStructType:
      case TokenUnionType:
         if (*Typ != NULL)
            ProgramFail(Parser, "bad type declaration");
         TypeParseStruct(Parser, Typ, Token == TokenStructType);
      break;
      case TokenEnumType:
         if (*Typ != NULL)
            ProgramFail(Parser, "bad type declaration");
         TypeParseEnum(Parser, Typ);
      break;
      case TokenIdentifier:
      // We already know it's a typedef-defined type because we got here.
         VariableGet(pc, Parser, LexerValue->Val->Identifier, &VarValue);
         *Typ = VarValue->Val->Typ;
      break;
      default:
         ParserCopy(Parser, &Before);
      return false;
   }
   return true;
}

// Parse a type - the part at the end after the identifier. e.g. array specifications etc.
ValueType TypeParseBack(ParseState Parser, ValueType FromType) {
   LexToken Token;
   struct ParseState Before;
   ParserCopy(&Before, Parser);
   Token = LexGetToken(Parser, NULL, true);
   if (Token == TokenLeftSquareBracket) {
   // Add another array bound.
      if (LexGetToken(Parser, NULL, false) == TokenRightSquareBracket) {
      // An unsized array.
         LexGetToken(Parser, NULL, true);
         return TypeGetMatching(Parser->pc, Parser, TypeParseBack(Parser, FromType), TypeArray, 0, Parser->pc->StrEmpty, true);
      } else {
      // Get a numeric array size.
         RunMode OldMode = Parser->Mode;
         int ArraySize;
         Parser->Mode = RunModeRun;
         ArraySize = ExpressionParseInt(Parser);
         Parser->Mode = OldMode;
         if (LexGetToken(Parser, NULL, true) != TokenRightSquareBracket)
            ProgramFail(Parser, "']' expected");
         return TypeGetMatching(Parser->pc, Parser, TypeParseBack(Parser, FromType), TypeArray, ArraySize, Parser->pc->StrEmpty, true);
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
   LexToken Token;
   Value LexValue;
   bool Done = false;
   *Typ = BasicTyp;
   *Identifier = Parser->pc->StrEmpty;
   while (!Done) {
      ParserCopy(&Before, Parser);
      Token = LexGetToken(Parser, &LexValue, true);
      switch (Token) {
         case TokenOpenBracket:
            if (*Typ != NULL)
               ProgramFail(Parser, "bad type declaration");
            TypeParse(Parser, Typ, Identifier, NULL);
            if (LexGetToken(Parser, NULL, true) != TokenCloseBracket)
               ProgramFail(Parser, "')' expected");
         break;
         case TokenAsterisk:
            if (*Typ == NULL)
               ProgramFail(Parser, "bad type declaration");
            *Typ = TypeGetMatching(Parser->pc, Parser, *Typ, TypePointer, 0, Parser->pc->StrEmpty, true);
         break;
         case TokenIdentifier:
            if (*Typ == NULL || *Identifier != Parser->pc->StrEmpty)
               ProgramFail(Parser, "bad type declaration");
            *Identifier = LexValue->Val->Identifier;
            Done = true;
         break;
         default:
            ParserCopy(Parser, &Before);
            Done = true;
         break;
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
   if (Typ->Base == TypeArray)
      return TypeIsForwardDeclared(Parser, Typ->FromType);
   if ((Typ->Base == TypeStruct || Typ->Base == TypeUnion) && Typ->Members == NULL)
      return true;
   return false;
}
