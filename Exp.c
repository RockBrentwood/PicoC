// PicoC expression evaluator: a stack-based expression evaluation system which handles operator precedence.
#include "Extern.h"

// Whether evaluation is left to right for a given precedence level.
#define RightWard(P) ((P) != 2 && (P) != 14)
static const unsigned short BracketLevel = 20;
static const unsigned short DeepPrecedence = 1000*BracketLevel;

#ifdef DEBUG_EXPRESSIONS
#   define DebugF printf
#else
static void DebugF(char *Format, ...) {
}
#endif

// Local prototypes.
typedef enum Fixity { NoFix, PreFix, InFix, PostFix } Fixity;

// A stack of expressions we use in evaluation.
typedef struct ExpressionStack *ExpressionStack;
struct ExpressionStack {
   ExpressionStack Next; // The next lower item on the stack.
   Value Val; // The value for this stack node.
   Lexical Op; // The operator.
   short unsigned int Precedence; // The operator precedence of this node.
   unsigned char Order; // The evaluation order of this operator.
};

// Operator precedence definitions.
struct OpPrecedence {
   unsigned int PrefixPrecedence:4;
   unsigned int PostfixPrecedence:4;
   unsigned int InfixPrecedence:4;
   char *Name;
};

// NOTE: the order of this array must correspond exactly to the order of these tokens in Lexical.
static struct OpPrecedence OperatorPrecedence[] = {
   { 0, 0, 0,"none"},	// NoneL.
   { 0, 0, 0,","},	// CommaL.
   { 0, 0, 2,"="},	// EquL.
   { 0, 0, 2,"+="},	// AddEquL.
   { 0, 0, 2,"-="},	// SubEquL.
   { 0, 0, 2,"*="},	// MulEquL.
   { 0, 0, 2,"/="},	// DivEquL.
   { 0, 0, 2,"%="},	// ModEquL.
   { 0, 0, 2,"<<="},	// ShLEquL.
   { 0, 0, 2,">>="},	// ShREquL.
   { 0, 0, 2,"&="},	// AndEquL.
   { 0, 0, 2,"|="},	// OrEquL.
   { 0, 0, 2,"^="},	// XOrEquL.
   { 0, 0, 3,"?"},	// QuestL.
   { 0, 0, 3,":"},	// ColonL.
   { 0, 0, 4,"||"},	// OrOrL.
   { 0, 0, 5,"&&"},	// AndAndL.
   { 0, 0, 6,"|"},	// OrL.
   { 0, 0, 7,"^"},	// XOrL.
   {14, 0, 8,"&"},	// AndL.
   { 0, 0, 9,"=="},	// RelEqL.
   { 0, 0, 9,"!="},	// RelNeL.
   { 0, 0,10,"<"},	// RelLtL.
   { 0, 0,10,">"},	// RelGtL.
   { 0, 0,10,"<="},	// RelLeL.
   { 0, 0,10,">="},	// RelGeL.
   { 0, 0,11,"<<"},	// ShLL.
   { 0, 0,11,">>"},	// ShRL.
   {14, 0,12,"+"},	// AddL.
   {14, 0,12,"-"},	// SubL.
   {14, 0,13,"*"},	// StarL.
   { 0, 0,13,"/"},	// DivL.
   { 0, 0,13,"%"},	// ModL.
   {14,15, 0,"++"},	// IncOpL.
   {14,15, 0,"--"},	// DecOpL.
   {14, 0, 0,"!"},	// NotL.
   {14, 0, 0,"~"},	// CplL.
   {14, 0, 0,"sizeof"},	// SizeOfL.
   {14, 0, 0,"cast"},	// CastL.
   { 0, 0,15,"["},	// LBrL.
   { 0,15, 0,"]"},	// RBrL.
   { 0, 0,15,"."},	// DotL.
   { 0, 0,15,"->"},	// ArrowL.
   {15, 0, 0,"("},	// LParL.
   { 0,15, 0,")"}	// RParL.
};

static void ExpressionParseFunctionCall(ParseState Parser, ExpressionStack *StackTop, const char *FuncName, bool RunIt);

#ifdef DEBUG_EXPRESSIONS
// Show the contents of the expression stack.
static void ExpressionStackShow(State pc, ExpressionStack StackTop) {
   printf("Expression stack [0x%lx,0x%lx]: ", (long)pc->HeapStackTop, (long)StackTop);
   while (StackTop != NULL) {
      if (StackTop->Order == NoFix) {
      // It's a value.
         printf("%s=", StackTop->Val->IsLValue? "lvalue": "value");
         switch (StackTop->Val->Typ->Base) {
            case VoidT: printf("void"); break;
            case IntT: printf("%d:int", StackTop->Val->Val->Integer); break;
            case ShortIntT: printf("%d:short", StackTop->Val->Val->ShortInteger); break;
            case CharT: printf("%d:char", StackTop->Val->Val->Character); break;
            case LongIntT: printf("%ld:long", StackTop->Val->Val->LongInteger); break;
            case ShortNatT: printf("%d:unsigned short", StackTop->Val->Val->UnsignedShortInteger); break;
            case NatT: printf("%d:unsigned int", StackTop->Val->Val->UnsignedInteger); break;
            case LongNatT: printf("%ld:unsigned long", StackTop->Val->Val->UnsignedLongInteger); break;
            case RatT: printf("%f:fp", StackTop->Val->Val->FP); break;
            case FunctionT: printf("%s:function", StackTop->Val->Val->Identifier); break;
            case MacroT: printf("%s:macro", StackTop->Val->Val->Identifier); break;
            case PointerT:
               if (StackTop->Val->Val->Pointer == NULL)
                  printf("ptr(NULL)");
               else if (StackTop->Val->Typ->FromType->Base == CharT)
                  printf("\"%s\":string", (char *)StackTop->Val->Val->Pointer);
               else
                  printf("ptr(0x%lx)", (long)StackTop->Val->Val->Pointer);
            break;
            case ArrayT: printf("array"); break;
            case StructT: printf("%s:struct", StackTop->Val->Val->Identifier); break;
            case UnionT: printf("%s:union", StackTop->Val->Val->Identifier); break;
            case EnumT: printf("%s:enum", StackTop->Val->Val->Identifier); break;
            case TypeT: PrintType(StackTop->Val->Val->Typ, pc->CStdOut), printf(":type"); break;
            default: printf("unknown"); break;
         }
         printf("[0x%lx,0x%lx]", (long)StackTop, (long)StackTop->Val);
      } else {
      // It's an operator.
         printf("op='%s' %s %d", OperatorPrecedence[(int)StackTop->Op].Name, StackTop->Order == PreFix? "prefix": StackTop->Order == PostFix? "postfix": "infix", StackTop->Precedence);
         printf("[0x%lx]", (long)StackTop);
      }
      StackTop = StackTop->Next;
      if (StackTop != NULL)
         printf(", ");
   }
   printf("\n");
}
#endif

static bool IsTypeToken(ParseState Parser, Lexical t, Value LexValue) {
   if (t >= IntL && t <= UnsignedL)
      return true; // Base type.
// typedef'ed type?
   if (t == IdL) { // See TypeParseFront(), case IdL and ParseTypedef().
      if (VariableDefined(Parser->pc, LexValue->Val->Pointer)) {
         Value VarValue;
         VariableGet(Parser->pc, Parser, LexValue->Val->Pointer, &VarValue);
         if (VarValue->Typ == &Parser->pc->TypeType)
            return true;
      }
   }
   return false;
}

long ExpressionCoerceInteger(Value Val) {
   switch (Val->Typ->Base) {
      case IntT: return (long)Val->Val->Integer;
      case CharT: return (long)Val->Val->Character;
      case ShortIntT: return (long)Val->Val->ShortInteger;
      case LongIntT: return (long)Val->Val->LongInteger;
      case NatT: return (long)Val->Val->UnsignedInteger;
      case ShortNatT: return (long)Val->Val->UnsignedShortInteger;
      case LongNatT: return (long)Val->Val->UnsignedLongInteger;
      case ByteT: return (long)Val->Val->UnsignedCharacter;
      case PointerT: return (long)Val->Val->Pointer;
#ifndef NO_FP
      case RatT: return (long)Val->Val->FP;
#endif
      default: return 0;
   }
}

unsigned long ExpressionCoerceUnsignedInteger(Value Val) {
   switch (Val->Typ->Base) {
      case IntT: return (unsigned long)Val->Val->Integer;
      case CharT: return (unsigned long)Val->Val->Character;
      case ShortIntT: return (unsigned long)Val->Val->ShortInteger;
      case LongIntT: return (unsigned long)Val->Val->LongInteger;
      case NatT: return (unsigned long)Val->Val->UnsignedInteger;
      case ShortNatT: return (unsigned long)Val->Val->UnsignedShortInteger;
      case LongNatT: return (unsigned long)Val->Val->UnsignedLongInteger;
      case ByteT: return (unsigned long)Val->Val->UnsignedCharacter;
      case PointerT: return (unsigned long)Val->Val->Pointer;
#ifndef NO_FP
      case RatT: return (unsigned long)Val->Val->FP;
#endif
      default: return 0;
   }
}

#ifndef NO_FP
double ExpressionCoerceFP(Value Val) {
#   ifndef BROKEN_FLOAT_CASTS
   int IntVal;
   unsigned UnsignedVal;
   switch (Val->Typ->Base) {
      case IntT: IntVal = Val->Val->Integer; return (double)IntVal;
      case CharT: IntVal = Val->Val->Character; return (double)IntVal;
      case ShortIntT: IntVal = Val->Val->ShortInteger; return (double)IntVal;
      case LongIntT: IntVal = Val->Val->LongInteger; return (double)IntVal;
      case NatT: UnsignedVal = Val->Val->UnsignedInteger; return (double)UnsignedVal;
      case ShortNatT: UnsignedVal = Val->Val->UnsignedShortInteger; return (double)UnsignedVal;
      case LongNatT: UnsignedVal = Val->Val->UnsignedLongInteger; return (double)UnsignedVal;
      case ByteT: UnsignedVal = Val->Val->UnsignedCharacter; return (double)UnsignedVal;
      case RatT: return Val->Val->FP;
      default: return 0.0;
   }
#   else
   switch (Val->Typ->Base) {
      case IntT: return (double)Val->Val->Integer;
      case CharT: return (double)Val->Val->Character;
      case ShortIntT: return (double)Val->Val->ShortInteger;
      case LongIntT: return (double)Val->Val->LongInteger;
      case NatT: return (double)Val->Val->UnsignedInteger;
      case ShortNatT: return (double)Val->Val->UnsignedShortInteger;
      case LongNatT: return (double)Val->Val->UnsignedLongInteger;
      case ByteT: return (double)Val->Val->UnsignedCharacter;
      case RatT: return (double)Val->Val->FP;
      default: return 0.0;
   }
#   endif
}
#endif

// Assign an integer value.
static long ExpressionAssignInt(ParseState Parser, Value DestValue, long FromInt, bool After) {
   if (!DestValue->IsLValue)
      ProgramFail(Parser, "can't assign to this");
   long Result = After? ExpressionCoerceInteger(DestValue): FromInt;
   switch (DestValue->Typ->Base) {
      case IntT: DestValue->Val->Integer = FromInt; break;
      case ShortIntT: DestValue->Val->ShortInteger = (short)FromInt; break;
      case CharT: DestValue->Val->Character = (char)FromInt; break;
      case LongIntT: DestValue->Val->LongInteger = (long)FromInt; break;
      case NatT: DestValue->Val->UnsignedInteger = (unsigned int)FromInt; break;
      case ShortNatT: DestValue->Val->UnsignedShortInteger = (unsigned short)FromInt; break;
      case LongNatT: DestValue->Val->UnsignedLongInteger = (unsigned long)FromInt; break;
      case ByteT: DestValue->Val->UnsignedCharacter = (unsigned char)FromInt; break;
      default: break;
   }
   return Result;
}

#ifndef NO_FP
// Assign a floating point value.
static double ExpressionAssignFP(ParseState Parser, Value DestValue, double FromFP) {
   if (!DestValue->IsLValue)
      ProgramFail(Parser, "can't assign to this");
   DestValue->Val->FP = FromFP;
   return FromFP;
}

// Convert the floating point rational value Val to an integer if the destination we are assigning it to is not float.
#   define SetRatOrInt(Q, Rat, Int, BotVal, IsInt, Val) ( \
       (IsInt = !IsRatVal(BotVal))? \
       (Int = ExpressionAssignInt((Q), (BotVal), (long)(Val), false)): \
       (Rat = ExpressionAssignFP((Q), (BotVal), (Val))) \
    )
#endif

// Push a node on to the expression stack.
static void ExpressionStackPushValueNode(ParseState Parser, ExpressionStack *StackTop, Value ValueLoc) {
   ExpressionStack StackNode = VariableAlloc(Parser->pc, Parser, sizeof *StackNode, false);
   StackNode->Next = *StackTop;
   StackNode->Val = ValueLoc;
   *StackTop = StackNode;
#ifdef FANCY_ERROR_MESSAGES
   StackNode->Line = Parser->Line;
   StackNode->CharacterPos = Parser->CharacterPos;
#endif
#ifdef DEBUG_EXPRESSIONS
   ExpressionStackShow(Parser->pc, *StackTop);
#endif
}

// Push a blank value on to the expression stack by type.
static Value ExpressionStackPushValueByType(ParseState Parser, ExpressionStack *StackTop, ValueType PushType) {
   Value ValueLoc = VariableAllocValueFromType(Parser->pc, Parser, PushType, false, NULL, false);
   ExpressionStackPushValueNode(Parser, StackTop, ValueLoc);
   return ValueLoc;
}

// Push a value on to the expression stack.
static void ExpressionStackPushValue(ParseState Parser, ExpressionStack *StackTop, Value PushValue) {
   Value ValueLoc = VariableAllocValueAndCopy(Parser->pc, Parser, PushValue, false);
   ExpressionStackPushValueNode(Parser, StackTop, ValueLoc);
}

static void ExpressionStackPushLValue(ParseState Parser, ExpressionStack *StackTop, Value PushValue, int Offset) {
   Value ValueLoc = VariableAllocValueShared(Parser, PushValue);
   ValueLoc->Val = (void *)((char *)ValueLoc->Val + Offset);
   ExpressionStackPushValueNode(Parser, StackTop, ValueLoc);
}

static void ExpressionStackPushDereference(ParseState Parser, ExpressionStack *StackTop, Value DereferenceValue) {
   Value DerefVal; int Offset; ValueType DerefType; bool DerefIsLValue;
   void *DerefDataLoc = VariableDereferencePointer(Parser, DereferenceValue, &DerefVal, &Offset, &DerefType, &DerefIsLValue);
   if (DerefDataLoc == NULL)
      ProgramFail(Parser, "NULL pointer dereference");
   Value ValueLoc = VariableAllocValueFromExistingData(Parser, DerefType, (AnyValue)DerefDataLoc, DerefIsLValue, DerefVal);
   ExpressionStackPushValueNode(Parser, StackTop, ValueLoc);
}

static void ExpressionPushInt(ParseState Parser, ExpressionStack *StackTop, long IntValue) {
   Value ValueLoc = VariableAllocValueFromType(Parser->pc, Parser, &Parser->pc->IntType, false, NULL, false);
   ValueLoc->Val->Integer = IntValue;
   ExpressionStackPushValueNode(Parser, StackTop, ValueLoc);
}

#ifndef NO_FP
void ExpressionPushFP(ParseState Parser, ExpressionStack *StackTop, double FPValue) {
   Value ValueLoc = VariableAllocValueFromType(Parser->pc, Parser, &Parser->pc->FPType, false, NULL, false);
   ValueLoc->Val->FP = FPValue;
   ExpressionStackPushValueNode(Parser, StackTop, ValueLoc);
}
#endif

// Assign to a pointer.
static void ExpressionAssignToPointer(ParseState Parser, Value ToValue, Value FromValue, const char *FuncName, int ParamNo, bool AllowPointerCoercion) {
   ValueType PointedToType = ToValue->Typ->FromType;
   if (FromValue->Typ == ToValue->Typ || FromValue->Typ == Parser->pc->VoidPtrType || (ToValue->Typ == Parser->pc->VoidPtrType && FromValue->Typ->Base == PointerT))
      ToValue->Val->Pointer = FromValue->Val->Pointer; // Plain old pointer assignment.
   else if (FromValue->Typ->Base == ArrayT && (PointedToType == FromValue->Typ->FromType || ToValue->Typ == Parser->pc->VoidPtrType)) {
   // The form is: blah *x = array of blah.
      ToValue->Val->Pointer = (void *)FromValue->Val->ArrayMem;
   } else if (FromValue->Typ->Base == PointerT && FromValue->Typ->FromType->Base == ArrayT && (PointedToType == FromValue->Typ->FromType->FromType || ToValue->Typ == Parser->pc->VoidPtrType)) {
   // The form is: blah *x = pointer to array of blah.
      ToValue->Val->Pointer = VariableDereferencePointer(Parser, FromValue, NULL, NULL, NULL, NULL);
   } else if (IsNumVal(FromValue) && ExpressionCoerceInteger(FromValue) == 0) {
   // Null pointer assignment.
      ToValue->Val->Pointer = NULL;
   } else if (AllowPointerCoercion && IsNumVal(FromValue)) {
   // Assign integer to native pointer.
      ToValue->Val->Pointer = (void *)(unsigned long)ExpressionCoerceUnsignedInteger(FromValue);
   } else if (AllowPointerCoercion && FromValue->Typ->Base == PointerT) {
   // Assign a pointer to a pointer to a different type.
      ToValue->Val->Pointer = FromValue->Val->Pointer;
   } else
      AssignFail(Parser, "%t from %t", ToValue->Typ, FromValue->Typ, 0, 0, FuncName, ParamNo);
}

// Assign any kind of value.
void ExpressionAssign(ParseState Parser, Value DestValue, Value SourceValue, bool Force, const char *FuncName, int ParamNo, bool AllowPointerCoercion) {
   if (!DestValue->IsLValue && !Force)
      AssignFail(Parser, "not an lvalue", NULL, NULL, 0, 0, FuncName, ParamNo);
   if (IsNumVal(DestValue) && !IsIntOrAddr(SourceValue, AllowPointerCoercion))
      AssignFail(Parser, "%t from %t", DestValue->Typ, SourceValue->Typ, 0, 0, FuncName, ParamNo);
   switch (DestValue->Typ->Base) {
      case IntT: DestValue->Val->Integer = ExpressionCoerceInteger(SourceValue); break;
      case ShortIntT: DestValue->Val->ShortInteger = (short)ExpressionCoerceInteger(SourceValue); break;
      case CharT: DestValue->Val->Character = (char)ExpressionCoerceInteger(SourceValue); break;
      case LongIntT: DestValue->Val->LongInteger = ExpressionCoerceInteger(SourceValue); break;
      case NatT: DestValue->Val->UnsignedInteger = ExpressionCoerceUnsignedInteger(SourceValue); break;
      case ShortNatT: DestValue->Val->UnsignedShortInteger = (unsigned short)ExpressionCoerceUnsignedInteger(SourceValue); break;
      case LongNatT: DestValue->Val->UnsignedLongInteger = ExpressionCoerceUnsignedInteger(SourceValue); break;
      case ByteT: DestValue->Val->UnsignedCharacter = (unsigned char)ExpressionCoerceUnsignedInteger(SourceValue); break;
#ifndef NO_FP
      case RatT:
         if (!IsIntOrAddr(SourceValue, AllowPointerCoercion))
            AssignFail(Parser, "%t from %t", DestValue->Typ, SourceValue->Typ, 0, 0, FuncName, ParamNo);
         DestValue->Val->FP = ExpressionCoerceFP(SourceValue);
      break;
#endif
      case PointerT: ExpressionAssignToPointer(Parser, DestValue, SourceValue, FuncName, ParamNo, AllowPointerCoercion);
      break;
      case ArrayT:
         if (SourceValue->Typ->Base == ArrayT/* && DestValue->Typ->FromType == DestValue->Typ->FromType*/ && DestValue->Typ->ArraySize == 0) {
         // Destination array is unsized - need to resize the destination array to the same size as the source array.
            DestValue->Typ = SourceValue->Typ;
            VariableRealloc(Parser, DestValue, TypeSizeValue(DestValue, false));
            if (DestValue->LValueFrom != NULL) {
            // Copy the resized value back to the LValue.
               DestValue->LValueFrom->Val = DestValue->Val;
               DestValue->LValueFrom->AnyValOnHeap = DestValue->AnyValOnHeap;
            }
         }
      // char array = "abcd".
         if (DestValue->Typ->FromType->Base == CharT && SourceValue->Typ->Base == PointerT && SourceValue->Typ->FromType->Base == CharT) {
            if (DestValue->Typ->ArraySize == 0) { // char x[] = "abcd", x is unsized.
               int Size = strlen(SourceValue->Val->Pointer) + 1;
#ifdef DEBUG_ARRAY_INITIALIZER
               ShowSourcePos(Parser);
               fprintf(stderr, "str size: %d\n", Size);
#endif
               DestValue->Typ = TypeGetMatching(Parser->pc, Parser, DestValue->Typ->FromType, DestValue->Typ->Base, Size, DestValue->Typ->Identifier, true);
               VariableRealloc(Parser, DestValue, TypeSizeValue(DestValue, false));
            }
         // Else, it's char x[10] = "abcd".
#ifdef DEBUG_ARRAY_INITIALIZER
            ShowSourcePos(Parser);
            fprintf(stderr, "char[%d] from char* (len=%d)\n", DestValue->Typ->ArraySize, strlen(SourceValue->Val->Pointer));
#endif
            memcpy((void *)DestValue->Val, SourceValue->Val->Pointer, TypeSizeValue(DestValue, false));
            break;
         }
         if (DestValue->Typ != SourceValue->Typ)
            AssignFail(Parser, "%t from %t", DestValue->Typ, SourceValue->Typ, 0, 0, FuncName, ParamNo);
         if (DestValue->Typ->ArraySize != SourceValue->Typ->ArraySize)
            AssignFail(Parser, "from an array of size %d to one of size %d", NULL, NULL, DestValue->Typ->ArraySize, SourceValue->Typ->ArraySize, FuncName, ParamNo);
         memcpy((void *)DestValue->Val, (void *)SourceValue->Val, TypeSizeValue(DestValue, false));
      break;
      case StructT: case UnionT:
         if (DestValue->Typ != SourceValue->Typ)
            AssignFail(Parser, "%t from %t", DestValue->Typ, SourceValue->Typ, 0, 0, FuncName, ParamNo);
         memcpy((void *)DestValue->Val, (void *)SourceValue->Val, TypeSizeValue(SourceValue, false));
      break;
      default: AssignFail(Parser, "%t", DestValue->Typ, NULL, 0, 0, FuncName, ParamNo); break;
   }
}

// Evaluate the first half of a ternary operator x ? y : z.
static void ExpressionQuestionMarkOperator(ParseState Parser, ExpressionStack *StackTop, Value BottomValue, Value TopValue) {
   if (!IsNumVal(TopValue))
      ProgramFail(Parser, "first argument to '?' should be a number");
   if (ExpressionCoerceInteger(TopValue)) {
   // The condition's true, return the BottomValue.
      ExpressionStackPushValue(Parser, StackTop, BottomValue);
   } else {
   // The condition's false, return void.
      ExpressionStackPushValueByType(Parser, StackTop, &Parser->pc->VoidType);
   }
}

// Evaluate the second half of a ternary operator x ? y : z.
static void ExpressionColonOperator(ParseState Parser, ExpressionStack *StackTop, Value BottomValue, Value TopValue) {
// void: Invoke the "else" part - return the BottomValue, else: It was a "then" - return the TopValue.
   ExpressionStackPushValue(Parser, StackTop, TopValue->Typ->Base == VoidT? BottomValue: TopValue);
}

// Evaluate a prefix operator.
static void ExpressionPrefixOperator(ParseState Parser, ExpressionStack *StackTop, Lexical Op, Value TopValue) {
   DebugF("ExpressionPrefixOperator()\n");
   switch (Op) {
      case AndL: {
         if (!TopValue->IsLValue)
            ProgramFail(Parser, "can't get the address of this");
         AnyValue ValPtr = TopValue->Val;
         Value Result = VariableAllocValueFromType(Parser->pc, Parser, TypeGetMatching(Parser->pc, Parser, TopValue->Typ, PointerT, 0, Parser->pc->StrEmpty, true), false, NULL, false);
         Result->Val->Pointer = (void *)ValPtr;
         ExpressionStackPushValueNode(Parser, StackTop, Result);
      }
      break;
      case StarL: ExpressionStackPushDereference(Parser, StackTop, TopValue); break;
      case SizeOfL:
      // Return the size of the argument.
         if (TopValue->Typ == &Parser->pc->TypeType)
            ExpressionPushInt(Parser, StackTop, TypeSize(TopValue->Val->Typ, TopValue->Val->Typ->ArraySize, true));
         else
            ExpressionPushInt(Parser, StackTop, TypeSize(TopValue->Typ, TopValue->Typ->ArraySize, true));
      break;
      default:
      // An arithmetic operator.
#ifndef NO_FP
         if (TopValue->Typ == &Parser->pc->FPType) {
         // Floating point prefix arithmetic.
            double ResultFP = 0.0;
            switch (Op) {
               case AddL: ResultFP = TopValue->Val->FP; break;
               case SubL: ResultFP = -TopValue->Val->FP; break;
               case IncOpL: ResultFP = ExpressionAssignFP(Parser, TopValue, TopValue->Val->FP + 1); break;
               case DecOpL: ResultFP = ExpressionAssignFP(Parser, TopValue, TopValue->Val->FP - 1); break;
               case NotL: ResultFP = !TopValue->Val->FP; break;
               default: ProgramFail(Parser, "invalid operation"); break;
            }
            ExpressionPushFP(Parser, StackTop, ResultFP);
         } else
#endif
         if (IsNumVal(TopValue)) {
         // Integer prefix arithmetic.
            long TopInt = ExpressionCoerceInteger(TopValue);
            long ResultInt = 0;
            switch (Op) {
               case AddL: ResultInt = TopInt; break;
               case SubL: ResultInt = -TopInt; break;
               case IncOpL: ResultInt = ExpressionAssignInt(Parser, TopValue, TopInt + 1, false); break;
               case DecOpL: ResultInt = ExpressionAssignInt(Parser, TopValue, TopInt - 1, false); break;
               case NotL: ResultInt = !TopInt; break;
               case CplL: ResultInt = ~TopInt; break;
               default: ProgramFail(Parser, "invalid operation"); break;
            }
            ExpressionPushInt(Parser, StackTop, ResultInt);
         } else if (TopValue->Typ->Base == PointerT) {
         // Pointer prefix arithmetic.
            int Size = TypeSize(TopValue->Typ->FromType, 0, true);
            if (TopValue->Val->Pointer == NULL)
               ProgramFail(Parser, "invalid use of a NULL pointer");
            if (!TopValue->IsLValue)
               ProgramFail(Parser, "can't assign to this");
            switch (Op) {
               case IncOpL: TopValue->Val->Pointer = (void *)((char *)TopValue->Val->Pointer + Size); break;
               case DecOpL: TopValue->Val->Pointer = (void *)((char *)TopValue->Val->Pointer - Size); break;
               default: ProgramFail(Parser, "invalid operation"); break;
            }
            void *ResultPtr = TopValue->Val->Pointer;
            Value StackValue = ExpressionStackPushValueByType(Parser, StackTop, TopValue->Typ);
            StackValue->Val->Pointer = ResultPtr;
         } else
            ProgramFail(Parser, "invalid operation");
      break;
   }
}

// Evaluate a postfix operator.
static void ExpressionPostfixOperator(ParseState Parser, ExpressionStack *StackTop, Lexical Op, Value TopValue) {
   DebugF("ExpressionPostfixOperator()\n");
#ifndef NO_FP
   if (TopValue->Typ == &Parser->pc->FPType) {
   // Floating point prefix arithmetic.
      double ResultFP = 0.0;
      switch (Op) {
         case IncOpL: ResultFP = ExpressionAssignFP(Parser, TopValue, TopValue->Val->FP + 1); break;
         case DecOpL: ResultFP = ExpressionAssignFP(Parser, TopValue, TopValue->Val->FP - 1); break;
         default: ProgramFail(Parser, "invalid operation"); break;
      }
      ExpressionPushFP(Parser, StackTop, ResultFP);
   } else
#endif
   if (IsNumVal(TopValue)) {
      long TopInt = ExpressionCoerceInteger(TopValue);
      long ResultInt = 0;
      switch (Op) {
         case IncOpL: ResultInt = ExpressionAssignInt(Parser, TopValue, TopInt + 1, true); break;
         case DecOpL: ResultInt = ExpressionAssignInt(Parser, TopValue, TopInt - 1, true); break;
         case RBrL: ProgramFail(Parser, "not supported"); break; // XXX.
         case RParL: ProgramFail(Parser, "not supported"); break; // XXX.
         default: ProgramFail(Parser, "invalid operation"); break;
      }
      ExpressionPushInt(Parser, StackTop, ResultInt);
   } else if (TopValue->Typ->Base == PointerT) {
   // Pointer postfix arithmetic.
      void *OrigPointer = TopValue->Val->Pointer;
      if (OrigPointer == NULL)
         ProgramFail(Parser, "invalid use of a NULL pointer");
      if (!TopValue->IsLValue)
         ProgramFail(Parser, "can't assign to this");
      int Size = TypeSize(TopValue->Typ->FromType, 0, true);
      switch (Op) {
         case IncOpL: TopValue->Val->Pointer = (void *)((char *)TopValue->Val->Pointer + Size); break;
         case DecOpL: TopValue->Val->Pointer = (void *)((char *)TopValue->Val->Pointer - Size); break;
         default: ProgramFail(Parser, "invalid operation"); break;
      }
      Value StackValue = ExpressionStackPushValueByType(Parser, StackTop, TopValue->Typ);
      StackValue->Val->Pointer = OrigPointer;
   } else
      ProgramFail(Parser, "invalid operation");
}

// Evaluate an infix operator.
static void ExpressionInfixOperator(ParseState Parser, ExpressionStack *StackTop, Lexical Op, Value BottomValue, Value TopValue) {
   DebugF("ExpressionInfixOperator()\n");
   if (BottomValue == NULL || TopValue == NULL)
      ProgramFail(Parser, "invalid expression");
   if (Op == LBrL) {
   // Array index.
      if (!IsNumVal(TopValue))
         ProgramFail(Parser, "array index must be an integer");
      int ArrayIndex = ExpressionCoerceInteger(TopValue);
   // Make the array element result.
      Value Result = NULL;
      switch (BottomValue->Typ->Base) {
         case ArrayT: Result = VariableAllocValueFromExistingData(Parser, BottomValue->Typ->FromType, (AnyValue)(BottomValue->Val->ArrayMem + TypeSize(BottomValue->Typ, ArrayIndex, true)), BottomValue->IsLValue, BottomValue->LValueFrom); break;
         case PointerT: Result = VariableAllocValueFromExistingData(Parser, BottomValue->Typ->FromType, (AnyValue)((char *)BottomValue->Val->Pointer + TypeSize(BottomValue->Typ->FromType, 0, true)*ArrayIndex), BottomValue->IsLValue, BottomValue->LValueFrom); break;
         default: ProgramFail(Parser, "this %t is not an array", BottomValue->Typ);
      }
      ExpressionStackPushValueNode(Parser, StackTop, Result);
   } else if (Op == QuestL)
      ExpressionQuestionMarkOperator(Parser, StackTop, TopValue, BottomValue);
   else if (Op == ColonL)
      ExpressionColonOperator(Parser, StackTop, TopValue, BottomValue);
#ifndef NO_FP
   else if ((TopValue->Typ == &Parser->pc->FPType && BottomValue->Typ == &Parser->pc->FPType) || (TopValue->Typ == &Parser->pc->FPType && IsNumVal(BottomValue)) || (IsNumVal(TopValue) && BottomValue->Typ == &Parser->pc->FPType)) {
   // Floating point infix arithmetic.
      double TopFP = TopValue->Typ == &Parser->pc->FPType? TopValue->Val->FP: (double)ExpressionCoerceInteger(TopValue);
      double BottomFP = BottomValue->Typ == &Parser->pc->FPType? BottomValue->Val->FP: (double)ExpressionCoerceInteger(BottomValue);
      double ResultFP = 0.0;
      long ResultInt = 0;
      bool ResultIsInt = false;
      switch (Op) {
         case EquL: SetRatOrInt(Parser, ResultFP, ResultInt, BottomValue, ResultIsInt, TopFP); break;
         case AddEquL: SetRatOrInt(Parser, ResultFP, ResultInt, BottomValue, ResultIsInt, BottomFP + TopFP); break;
         case SubEquL: SetRatOrInt(Parser, ResultFP, ResultInt, BottomValue, ResultIsInt, BottomFP - TopFP); break;
         case MulEquL: SetRatOrInt(Parser, ResultFP, ResultInt, BottomValue, ResultIsInt, BottomFP * TopFP); break;
         case DivEquL: SetRatOrInt(Parser, ResultFP, ResultInt, BottomValue, ResultIsInt, BottomFP / TopFP); break;
         case RelEqL: ResultInt = BottomFP == TopFP, ResultIsInt = true; break;
         case RelNeL: ResultInt = BottomFP != TopFP, ResultIsInt = true; break;
         case RelLtL: ResultInt = BottomFP < TopFP, ResultIsInt = true; break;
         case RelGtL: ResultInt = BottomFP > TopFP, ResultIsInt = true; break;
         case RelLeL: ResultInt = BottomFP <= TopFP, ResultIsInt = true; break;
         case RelGeL: ResultInt = BottomFP >= TopFP, ResultIsInt = true; break;
         case AddL: ResultFP = BottomFP + TopFP; break;
         case SubL: ResultFP = BottomFP - TopFP; break;
         case StarL: ResultFP = BottomFP * TopFP; break;
         case DivL: ResultFP = BottomFP / TopFP; break;
         default: ProgramFail(Parser, "invalid operation"); break;
      }
      if (ResultIsInt)
         ExpressionPushInt(Parser, StackTop, ResultInt);
      else
         ExpressionPushFP(Parser, StackTop, ResultFP);
   }
#endif
   else if (IsNumVal(TopValue) && IsNumVal(BottomValue)) {
   // Integer operation.
      long TopInt = ExpressionCoerceInteger(TopValue);
      long BottomInt = ExpressionCoerceInteger(BottomValue);
      long ResultInt = 0;
      switch (Op) {
         case EquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, TopInt, false); break;
         case AddEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt + TopInt, false); break;
         case SubEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt - TopInt, false); break;
         case MulEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt * TopInt, false); break;
         case DivEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt / TopInt, false); break;
#ifndef NO_MODULUS
         case ModEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt % TopInt, false); break;
#endif
         case ShLEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt << TopInt, false); break;
         case ShREquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt >> TopInt, false); break;
         case AndEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt & TopInt, false); break;
         case OrEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt | TopInt, false); break;
         case XOrEquL: ResultInt = ExpressionAssignInt(Parser, BottomValue, BottomInt ^ TopInt, false); break;
         case OrOrL: ResultInt = BottomInt || TopInt; break;
         case AndAndL: ResultInt = BottomInt && TopInt; break;
         case OrL: ResultInt = BottomInt | TopInt; break;
         case XOrL: ResultInt = BottomInt ^ TopInt; break;
         case AndL: ResultInt = BottomInt & TopInt; break;
         case RelEqL: ResultInt = BottomInt == TopInt; break;
         case RelNeL: ResultInt = BottomInt != TopInt; break;
         case RelLtL: ResultInt = BottomInt < TopInt; break;
         case RelGtL: ResultInt = BottomInt > TopInt; break;
         case RelLeL: ResultInt = BottomInt <= TopInt; break;
         case RelGeL: ResultInt = BottomInt >= TopInt; break;
         case ShLL: ResultInt = BottomInt << TopInt; break;
         case ShRL: ResultInt = BottomInt >> TopInt; break;
         case AddL: ResultInt = BottomInt + TopInt; break;
         case SubL: ResultInt = BottomInt - TopInt; break;
         case StarL: ResultInt = BottomInt * TopInt; break;
         case DivL: ResultInt = BottomInt / TopInt; break;
#ifndef NO_MODULUS
         case ModL: ResultInt = BottomInt % TopInt; break;
#endif
         default: ProgramFail(Parser, "invalid operation"); break;
      }
      ExpressionPushInt(Parser, StackTop, ResultInt);
   } else if (BottomValue->Typ->Base == PointerT && IsNumVal(TopValue)) {
   // Pointer/integer infix arithmetic.
      long TopInt = ExpressionCoerceInteger(TopValue);
      if (Op == RelEqL || Op == RelNeL) {
      // Comparison to a NULL pointer.
         if (TopInt != 0)
            ProgramFail(Parser, "invalid operation");
         ExpressionPushInt(Parser, StackTop, (Op == RelEqL) == (BottomValue->Val->Pointer == NULL));
      } else if (Op == AddL || Op == SubL) {
      // Pointer arithmetic.
         int Size = TypeSize(BottomValue->Typ->FromType, 0, true);
         void *Pointer = BottomValue->Val->Pointer;
         if (Pointer == NULL)
            ProgramFail(Parser, "invalid use of a NULL pointer");
         Pointer = (void *)(Op == AddL? (char *)Pointer + TopInt*Size: (char *)Pointer - TopInt*Size);
         Value StackValue = ExpressionStackPushValueByType(Parser, StackTop, BottomValue->Typ);
         StackValue->Val->Pointer = Pointer;
      } else if (Op == EquL && TopInt == 0) {
      // Assign a NULL pointer.
         HeapUnpopStack(Parser->pc, sizeof(struct Value));
         ExpressionAssign(Parser, BottomValue, TopValue, false, NULL, 0, false);
         ExpressionStackPushValueNode(Parser, StackTop, BottomValue);
      } else if (Op == AddEquL || Op == SubEquL) {
      // Pointer arithmetic.
         int Size = TypeSize(BottomValue->Typ->FromType, 0, true);
         void *Pointer = BottomValue->Val->Pointer;
         if (Pointer == NULL)
            ProgramFail(Parser, "invalid use of a NULL pointer");
         Pointer = (void *)(Op == AddEquL? (char *)Pointer + TopInt*Size: (char *)Pointer - TopInt*Size);
         HeapUnpopStack(Parser->pc, sizeof(struct Value));
         BottomValue->Val->Pointer = Pointer;
         ExpressionStackPushValueNode(Parser, StackTop, BottomValue);
      } else
         ProgramFail(Parser, "invalid operation");
   } else if (BottomValue->Typ->Base == PointerT && TopValue->Typ->Base == PointerT && Op != EquL) {
   // Pointer/pointer operations.
      char *TopLoc = (char *)TopValue->Val->Pointer;
      char *BottomLoc = (char *)BottomValue->Val->Pointer;
      switch (Op) {
         case RelEqL: ExpressionPushInt(Parser, StackTop, BottomLoc == TopLoc); break;
         case RelNeL: ExpressionPushInt(Parser, StackTop, BottomLoc != TopLoc); break;
         case SubL: ExpressionPushInt(Parser, StackTop, BottomLoc - TopLoc); break;
         default: ProgramFail(Parser, "invalid operation"); break;
      }
   } else if (Op == EquL) {
   // Assign a non-numeric type.
      HeapUnpopStack(Parser->pc, sizeof(struct Value)); // XXX - possible bug if lvalue is a temp value and takes more than sizeof(struct Value).
      ExpressionAssign(Parser, BottomValue, TopValue, false, NULL, 0, false);
      ExpressionStackPushValueNode(Parser, StackTop, BottomValue);
   } else if (Op == CastL) {
   // Cast a value to a different type.
   // XXX - possible bug if the destination type takes more than sizeof(struct Value) + sizeof(ValueType).
      Value ValueLoc = ExpressionStackPushValueByType(Parser, StackTop, BottomValue->Val->Typ);
      ExpressionAssign(Parser, ValueLoc, TopValue, true, NULL, 0, true);
   } else
      ProgramFail(Parser, "invalid operation");
}

// Take the contents of the expression stack and compute the top until there's nothing greater than the given precedence.
static void ExpressionStackCollapse(ParseState Parser, ExpressionStack *StackTop, int Precedence, int *IgnorePrecedence) {
   DebugF("ExpressionStackCollapse(%d):\n", Precedence);
   ExpressionStack TopStackNode = *StackTop;
#ifdef DEBUG_EXPRESSIONS
   ExpressionStackShow(Parser->pc, TopStackNode);
#endif
   int FoundPrecedence = Precedence;
   while (TopStackNode != NULL && TopStackNode->Next != NULL && FoundPrecedence >= Precedence) {
   // Find the top operator on the stack.
      ExpressionStack TopOperatorNode = TopStackNode->Order == NoFix? TopStackNode->Next: TopStackNode;
      FoundPrecedence = TopOperatorNode->Precedence;
   // Does it have a high enough precedence?
      if (FoundPrecedence >= Precedence && TopOperatorNode != NULL) {
      // Execute this operator.
         switch (TopOperatorNode->Order) {
         // Prefix evaluation.
            case PreFix: {
               DebugF("prefix evaluation\n");
               Value TopValue = TopStackNode->Val;
            // Pop the value and then the prefix operator - assume they'll still be there until we're done.
               HeapPopStack(Parser->pc, NULL, sizeof *TopStackNode + sizeof *TopValue + TypeStackSizeValue(TopValue));
               HeapPopStack(Parser->pc, TopOperatorNode, sizeof *TopStackNode);
               *StackTop = TopOperatorNode->Next;
            // Do the prefix operation.
               if (Parser->Mode == RunM/* && FoundPrecedence < *IgnorePrecedence*/) {
               // Run the operator.
                  ExpressionPrefixOperator(Parser, StackTop, TopOperatorNode->Op, TopValue);
               } else {
               // We're not running it so just return 0.
                  ExpressionPushInt(Parser, StackTop, 0);
               }
            }
            break;
         // Postfix evaluation.
            case PostFix: {
               DebugF("postfix evaluation\n");
               Value TopValue = TopStackNode->Next->Val;
            // Pop the postfix operator and then the value - assume they'll still be there until we're done.
               HeapPopStack(Parser->pc, NULL, sizeof *TopStackNode);
               HeapPopStack(Parser->pc, TopValue, sizeof *TopStackNode + sizeof *TopValue + TypeStackSizeValue(TopValue));
               *StackTop = TopStackNode->Next->Next;
            // Do the postfix operation.
               if (Parser->Mode == RunM/* && FoundPrecedence < *IgnorePrecedence*/) {
               // Run the operator.
                  ExpressionPostfixOperator(Parser, StackTop, TopOperatorNode->Op, TopValue);
               } else {
               // We're not running it so just return 0.
                  ExpressionPushInt(Parser, StackTop, 0);
               }
            }
            break;
         // Infix evaluation.
            case InFix: {
               DebugF("infix evaluation\n");
               Value TopValue = TopStackNode->Val;
               if (TopValue != NULL) {
                  Value BottomValue = TopOperatorNode->Next->Val;
               // Pop a value, the operator and another value - assume they'll still be there until we're done.
                  HeapPopStack(Parser->pc, NULL, sizeof *TopOperatorNode + sizeof *TopValue + TypeStackSizeValue(TopValue));
                  HeapPopStack(Parser->pc, NULL, sizeof *TopOperatorNode);
                  HeapPopStack(Parser->pc, BottomValue, sizeof *TopOperatorNode + sizeof *BottomValue + TypeStackSizeValue(BottomValue));
                  *StackTop = TopOperatorNode->Next->Next;
               // Do the infix operation.
                  if (Parser->Mode == RunM/* && FoundPrecedence <= *IgnorePrecedence*/) {
                  // Run the operator.
                     ExpressionInfixOperator(Parser, StackTop, TopOperatorNode->Op, BottomValue, TopValue);
                  } else {
                  // We're not running it so just return 0.
                     ExpressionPushInt(Parser, StackTop, 0);
                  }
               } else
                  FoundPrecedence = -1;
            }
            break;
         // This should never happen.
            case NoFix: assert(TopOperatorNode->Order != NoFix); break;
         }
      // If we've returned above the ignored precedence level turn ignoring off.
         if (FoundPrecedence <= *IgnorePrecedence)
            *IgnorePrecedence = DeepPrecedence;
      }
#ifdef DEBUG_EXPRESSIONS
      ExpressionStackShow(Parser->pc, *StackTop);
#endif
      TopStackNode = *StackTop;
   }
   DebugF("ExpressionStackCollapse() finished\n");
#ifdef DEBUG_EXPRESSIONS
   ExpressionStackShow(Parser->pc, *StackTop);
#endif
}

// Push an operator on to the expression stack.
static void ExpressionStackPushOperator(ParseState Parser, ExpressionStack *StackTop, Fixity Order, Lexical Token, int Precedence) {
   ExpressionStack StackNode = VariableAlloc(Parser->pc, Parser, sizeof **StackTop, false);
   StackNode->Next = *StackTop;
   StackNode->Order = Order;
   StackNode->Op = Token;
   StackNode->Precedence = Precedence;
   *StackTop = StackNode;
   DebugF("ExpressionStackPushOperator()\n");
#ifdef FANCY_ERROR_MESSAGES
   StackNode->Line = Parser->Line;
   StackNode->CharacterPos = Parser->CharacterPos;
#endif
#ifdef DEBUG_EXPRESSIONS
   ExpressionStackShow(Parser->pc, *StackTop);
#endif
}

// Do the '.' and '->' operators.
static void ExpressionGetStructElement(ParseState Parser, ExpressionStack *StackTop, Lexical Token) {
// Get the identifier following the '.' or '->'.
   Value Ident;
   if (LexGetToken(Parser, &Ident, true) != IdL)
      ProgramFail(Parser, "need an structure or union member after '%s'", Token == DotL? ".": "->");
   if (Parser->Mode == RunM) {
   // Look up the struct element.
      Value ParamVal = (*StackTop)->Val;
      Value StructVal = ParamVal;
      ValueType StructType = ParamVal->Typ;
   // If we're doing '->' dereference the struct pointer first.
      char *DerefDataLoc = (char *)ParamVal->Val;
      if (Token == ArrowL)
         DerefDataLoc = VariableDereferencePointer(Parser, ParamVal, &StructVal, NULL, &StructType, NULL);
      if (StructType->Base != StructT && StructType->Base != UnionT)
         ProgramFail(Parser, "can't use '%s' on something that's not a struct or union %s: it's a %t", Token == DotL? ".": "->", Token == ArrowL? "pointer": "", ParamVal->Typ);
      Value MemberValue = NULL;
      if (!TableGet(StructType->Members, Ident->Val->Identifier, &MemberValue, NULL, NULL, NULL))
         ProgramFail(Parser, "doesn't have a member called '%s'", Ident->Val->Identifier);
   // Pop the value - assume it'll still be there until we're done.
      HeapPopStack(Parser->pc, ParamVal, sizeof **StackTop + sizeof *StructVal + TypeStackSizeValue(StructVal));
      *StackTop = (*StackTop)->Next;
   // Make the result value for this member only.
      Value Result = VariableAllocValueFromExistingData(Parser, MemberValue->Typ, (void *)(DerefDataLoc + MemberValue->Val->Integer), true, StructVal != NULL? StructVal->LValueFrom: NULL);
      ExpressionStackPushValueNode(Parser, StackTop, Result);
   }
}

// Parse an expression with operator precedence.
bool ExpressionParse(ParseState Parser, Value *Result) {
   DebugF("ExpressionParse():\n");
   bool PrefixState = true;
   bool Done = false;
   int BracketPrecedence = 0;
   int Precedence = 0;
   int IgnorePrecedence = DeepPrecedence;
   ExpressionStack StackTop = NULL;
   int TernaryDepth = 0;
   do {
      struct ParseState PreState;
      ParserCopy(&PreState, Parser);
      Value LexValue;
      Lexical Token = LexGetToken(Parser, &LexValue, true);
      if ((((int)Token > CommaL && (int)Token <= (int)LParL) || (Token == RParL && BracketPrecedence != 0)) && (Token != ColonL || TernaryDepth > 0)) {
      // It's an operator with precedence.
         if (PrefixState) {
         // Expect a prefix operator.
            if (OperatorPrecedence[(int)Token].PrefixPrecedence == 0)
               ProgramFail(Parser, "operator not expected here");
            int LocalPrecedence = OperatorPrecedence[(int)Token].PrefixPrecedence;
            Precedence = BracketPrecedence + LocalPrecedence;
            if (Token == LParL) {
            // It's either a new bracket level or a cast.
               Lexical BracketToken = LexGetToken(Parser, &LexValue, false);
               if (IsTypeToken(Parser, BracketToken, LexValue) && (StackTop == NULL || StackTop->Op != SizeOfL)) {
               // It's a cast - get the new type.
                  ValueType CastType; char *CastIdentifier;
                  TypeParse(Parser, &CastType, &CastIdentifier, NULL);
                  if (LexGetToken(Parser, &LexValue, true) != RParL)
                     ProgramFail(Parser, "brackets not closed");
               // Scan and collapse the stack to the precedence of this infix cast operator, then push.
                  Precedence = BracketPrecedence + OperatorPrecedence[(int)CastL].PrefixPrecedence;
                  ExpressionStackCollapse(Parser, &StackTop, Precedence + 1, &IgnorePrecedence);
                  Value CastTypeValue = VariableAllocValueFromType(Parser->pc, Parser, &Parser->pc->TypeType, false, NULL, false);
                  CastTypeValue->Val->Typ = CastType;
                  ExpressionStackPushValueNode(Parser, &StackTop, CastTypeValue);
                  ExpressionStackPushOperator(Parser, &StackTop, InFix, CastL, Precedence);
               } else {
               // Boost the bracket operator precedence.
                  BracketPrecedence += BracketLevel;
               }
            } else {
            // Scan and collapse the stack to the precedence of this operator, then push.
            // Take some extra care for double prefix operators, e.g. x = - -5, or x = **y.
               int TempPrecedenceBoost = 0;
               int NextToken = LexGetToken(Parser, NULL, false);
               if (NextToken > CommaL && NextToken < LParL) {
                  int NextPrecedence = OperatorPrecedence[(int)NextToken].PrefixPrecedence;
               // Two prefix operators with equal precedence?
	       // Make sure the innermost one runs first.
               // XXX - probably not correct, but can't find a test that fails at this.
                  if (LocalPrecedence == NextPrecedence)
                     TempPrecedenceBoost = -1;
               }
               ExpressionStackCollapse(Parser, &StackTop, Precedence, &IgnorePrecedence);
               ExpressionStackPushOperator(Parser, &StackTop, PreFix, Token, Precedence + TempPrecedenceBoost);
            }
         } else {
         // Expect an infix or postfix operator.
            if (OperatorPrecedence[(int)Token].PostfixPrecedence != 0) {
               switch (Token) {
                  case RParL: case RBrL:
                     if (BracketPrecedence == 0) {
                     // Assume this bracket is after the end of the expression.
                        ParserCopy(Parser, &PreState);
                        Done = true;
                     } else {
                     // Collapse to the bracket precedence.
                        ExpressionStackCollapse(Parser, &StackTop, BracketPrecedence, &IgnorePrecedence);
                        BracketPrecedence -= BracketLevel;
                     }
                  break;
                  default:
                  // Scan and collapse the stack to the precedence of this operator, then push.
                     Precedence = BracketPrecedence + OperatorPrecedence[(int)Token].PostfixPrecedence;
                     ExpressionStackCollapse(Parser, &StackTop, Precedence, &IgnorePrecedence);
                     ExpressionStackPushOperator(Parser, &StackTop, PostFix, Token, Precedence);
                  break;
               }
            } else if (OperatorPrecedence[(int)Token].InfixPrecedence != 0) {
            // Scan and collapse the stack, then push.
               Precedence = BracketPrecedence + OperatorPrecedence[(int)Token].InfixPrecedence;
            // For right to left order, only go down to the next higher precedence so we evaluate it in reverse order.
            // For left to right order, collapse down to this precedence so we evaluate it in forward order.
               ExpressionStackCollapse(Parser, &StackTop, RightWard(OperatorPrecedence[(int)Token].InfixPrecedence)? Precedence: Precedence + 1, &IgnorePrecedence);
               if (Token == DotL || Token == ArrowL) {
                  ExpressionGetStructElement(Parser, &StackTop, Token); // This operator is followed by a struct element so handle it as a special case.
               } else {
               // If it's a && or || operator we may not need to evaluate the right hand side of the expression.
                  if ((Token == OrOrL || Token == AndAndL) && IsNumVal(StackTop->Val)) {
                     long LHSInt = ExpressionCoerceInteger(StackTop->Val);
                     if (((Token == OrOrL && LHSInt) || (Token == AndAndL && !LHSInt)) && IgnorePrecedence > Precedence)
                        IgnorePrecedence = Precedence;
                  }
               // Push the operator on the stack.
                  ExpressionStackPushOperator(Parser, &StackTop, InFix, Token, Precedence);
                  PrefixState = true;
                  switch (Token) {
                     case QuestL: TernaryDepth++; break;
                     case ColonL: TernaryDepth--; break;
                     default: break;
                  }
               }
            // Treat an open square bracket as an infix array index operator followed by an open bracket.
               if (Token == LBrL) {
               // Boost the bracket operator precedence, then push.
                  BracketPrecedence += BracketLevel;
               }
            } else
               ProgramFail(Parser, "operator not expected here");
         }
      } else if (Token == IdL) {
      // It's a variable, function or a macro.
         if (!PrefixState)
            ProgramFail(Parser, "identifier not expected here");
         if (LexGetToken(Parser, NULL, false) == LParL) {
            ExpressionParseFunctionCall(Parser, &StackTop, LexValue->Val->Identifier, Parser->Mode == RunM && Precedence < IgnorePrecedence);
         } else {
            if (Parser->Mode == RunM/* && Precedence < IgnorePrecedence*/) {
               Value VariableValue = NULL;
               VariableGet(Parser->pc, Parser, LexValue->Val->Identifier, &VariableValue);
               if (VariableValue->Typ->Base == MacroT) {
               // Evaluate a macro as a kind of simple subroutine.
                  struct ParseState MacroParser;
                  ParserCopy(&MacroParser, &VariableValue->Val->MacroDef.Body);
                  MacroParser.Mode = Parser->Mode;
                  if (VariableValue->Val->MacroDef.NumParams != 0)
                     ProgramFail(&MacroParser, "macro arguments missing");
                  Value MacroResult;
                  if (!ExpressionParse(&MacroParser, &MacroResult) || LexGetToken(&MacroParser, NULL, false) != EndFnL)
                     ProgramFail(&MacroParser, "expression expected");
                  ExpressionStackPushValueNode(Parser, &StackTop, MacroResult);
               } else if (VariableValue->Typ == &Parser->pc->VoidType)
                  ProgramFail(Parser, "a void value isn't much use here");
               else
                  ExpressionStackPushLValue(Parser, &StackTop, VariableValue, 0); // It's a value variable.
            } else // Push a dummy value.
               ExpressionPushInt(Parser, &StackTop, 0);
         }
      // If we've successfully ignored the RHS turn ignoring off.
         if (Precedence <= IgnorePrecedence)
            IgnorePrecedence = DeepPrecedence;
         PrefixState = false;
      } else if ((int)Token > RParL && (int)Token <= CharLitL) {
      // It's a value of some sort, push it.
         if (!PrefixState)
            ProgramFail(Parser, "value not expected here");
         PrefixState = false;
         ExpressionStackPushValue(Parser, &StackTop, LexValue);
      } else if (IsTypeToken(Parser, Token, LexValue)) {
      // It's a type.
      // Push it on the stack like a value.
      // This is used in sizeof().
         if (!PrefixState)
            ProgramFail(Parser, "type not expected here");
         PrefixState = false;
         ParserCopy(Parser, &PreState);
         ValueType Typ; char *Identifier;
         TypeParse(Parser, &Typ, &Identifier, NULL);
         Value TypeValue = VariableAllocValueFromType(Parser->pc, Parser, &Parser->pc->TypeType, false, NULL, false);
         TypeValue->Val->Typ = Typ;
         ExpressionStackPushValueNode(Parser, &StackTop, TypeValue);
      } else {
      // It isn't a token from an expression.
         ParserCopy(Parser, &PreState);
         Done = true;
      }
   } while (!Done);
// Check that brackets have been closed.
   if (BracketPrecedence > 0)
      ProgramFail(Parser, "brackets not closed");
// Scan and collapse the stack to precedence 0.
   ExpressionStackCollapse(Parser, &StackTop, 0, &IgnorePrecedence);
// Fix up the stack and return the result if we're in run mode.
   if (StackTop != NULL) {
   // All that should be left is a single value on the stack.
      if (Parser->Mode == RunM) {
         if (StackTop->Order != NoFix || StackTop->Next != NULL)
            ProgramFail(Parser, "invalid expression");
         *Result = StackTop->Val;
         HeapPopStack(Parser->pc, StackTop, sizeof *StackTop);
      } else
         HeapPopStack(Parser->pc, StackTop->Val, sizeof *StackTop + sizeof *StackTop->Val + TypeStackSizeValue(StackTop->Val));
   }
   DebugF("ExpressionParse() done\n\n");
#ifdef DEBUG_EXPRESSIONS
   ExpressionStackShow(Parser->pc, StackTop);
#endif
   return StackTop != NULL;
}

// Do a parameterized macro call.
static void ExpressionParseMacroCall(ParseState Parser, ExpressionStack *StackTop, const char *MacroName, MacroDef MDef) {
   Value ReturnValue = NULL;
   Value *ParamArray = NULL;
   if (Parser->Mode == RunM) {
   // Create a stack frame for this macro.
#ifndef NO_FP
      ExpressionStackPushValueByType(Parser, StackTop, &Parser->pc->FPType); // Largest return type there is.
#else
      ExpressionStackPushValueByType(Parser, StackTop, &Parser->pc->IntType); // Largest return type there is.
#endif
      ReturnValue = (*StackTop)->Val;
      HeapPushStackFrame(Parser->pc);
      ParamArray = HeapAllocStack(Parser->pc, MDef->NumParams*sizeof *ParamArray);
      if (ParamArray == NULL)
         ProgramFail(Parser, "out of memory");
   } else
      ExpressionPushInt(Parser, StackTop, 0);
// Parse arguments.
   int ArgCount = 0;
   Lexical Token;
   do {
      Value Param;
      if (ExpressionParse(Parser, &Param)) {
         if (Parser->Mode == RunM) {
            if (ArgCount < MDef->NumParams)
               ParamArray[ArgCount] = Param;
            else
               ProgramFail(Parser, "too many arguments to %s()", MacroName);
         }
         ArgCount++;
         Token = LexGetToken(Parser, NULL, true);
         if (Token != CommaL && Token != RParL)
            ProgramFail(Parser, "comma expected");
      } else {
      // End of argument list?
         Token = LexGetToken(Parser, NULL, true);
         if (!RParL)
            ProgramFail(Parser, "bad argument");
      }
   } while (Token != RParL);
   if (Parser->Mode == RunM) {
   // Evaluate the macro.
      if (ArgCount < MDef->NumParams)
         ProgramFail(Parser, "not enough arguments to '%s'", MacroName);
      if (MDef->Body.Pos == NULL)
         ProgramFail(Parser, "'%s' is undefined", MacroName);
      struct ParseState MacroParser;
      ParserCopy(&MacroParser, &MDef->Body);
      MacroParser.Mode = Parser->Mode;
      VariableStackFrameAdd(Parser, MacroName, 0);
      Parser->pc->TopStackFrame->NumParams = ArgCount;
      Parser->pc->TopStackFrame->ReturnValue = ReturnValue;
      for (int Count = 0; Count < MDef->NumParams; Count++)
         VariableDefine(Parser->pc, Parser, MDef->ParamName[Count], ParamArray[Count], NULL, true);
      Value EvalValue;
      ExpressionParse(&MacroParser, &EvalValue);
      ExpressionAssign(Parser, ReturnValue, EvalValue, true, MacroName, 0, false);
      VariableStackFramePop(Parser);
      HeapPopStackFrame(Parser->pc);
   }
}

// Do a function call.
static void ExpressionParseFunctionCall(ParseState Parser, ExpressionStack *StackTop, const char *FuncName, bool RunIt) {
   Lexical Token = LexGetToken(Parser, NULL, true); // Open bracket.
   RunMode OldMode = Parser->Mode;
   Value ReturnValue = NULL;
   Value FuncValue = NULL;
   Value *ParamArray = NULL;
   if (RunIt) {
   // Get the function definition.
      VariableGet(Parser->pc, Parser, FuncName, &FuncValue);
      if (FuncValue->Typ->Base == MacroT) {
      // This is actually a macro, not a function.
         ExpressionParseMacroCall(Parser, StackTop, FuncName, &FuncValue->Val->MacroDef);
         return;
      }
      if (FuncValue->Typ->Base != FunctionT)
         ProgramFail(Parser, "%t is not a function - can't call", FuncValue->Typ);
      ExpressionStackPushValueByType(Parser, StackTop, FuncValue->Val->FuncDef.ReturnType);
      ReturnValue = (*StackTop)->Val;
      HeapPushStackFrame(Parser->pc);
      ParamArray = HeapAllocStack(Parser->pc, FuncValue->Val->FuncDef.NumParams*sizeof *ParamArray);
      if (ParamArray == NULL)
         ProgramFail(Parser, "out of memory");
   } else {
      ExpressionPushInt(Parser, StackTop, 0);
      Parser->Mode = SkipM;
   }
// Parse arguments.
   int ArgCount = 0;
   do {
      if (RunIt && ArgCount < FuncValue->Val->FuncDef.NumParams)
         ParamArray[ArgCount] = VariableAllocValueFromType(Parser->pc, Parser, FuncValue->Val->FuncDef.ParamType[ArgCount], false, NULL, false);
      Value Param;
      if (ExpressionParse(Parser, &Param)) {
         if (RunIt) {
            if (ArgCount < FuncValue->Val->FuncDef.NumParams) {
               ExpressionAssign(Parser, ParamArray[ArgCount], Param, true, FuncName, ArgCount + 1, false);
               VariableStackPop(Parser, Param);
            } else {
               if (!FuncValue->Val->FuncDef.VarArgs)
                  ProgramFail(Parser, "too many arguments to %s()", FuncName);
            }
         }
         ArgCount++;
         Token = LexGetToken(Parser, NULL, true);
         if (Token != CommaL && Token != RParL)
            ProgramFail(Parser, "comma expected");
      } else {
      // End of argument list?
         Token = LexGetToken(Parser, NULL, true);
         if (!RParL)
            ProgramFail(Parser, "bad argument");
      }
   } while (Token != RParL);
   if (RunIt) {
   // Run the function.
      if (ArgCount < FuncValue->Val->FuncDef.NumParams)
         ProgramFail(Parser, "not enough arguments to '%s'", FuncName);
      if (FuncValue->Val->FuncDef.Intrinsic == NULL) {
      // Run a user-defined function.
         int OldScopeID = Parser->ScopeID;
         if (FuncValue->Val->FuncDef.Body.Pos == NULL)
            ProgramFail(Parser, "'%s' is undefined", FuncName);
         struct ParseState FuncParser;
         ParserCopy(&FuncParser, &FuncValue->Val->FuncDef.Body);
         VariableStackFrameAdd(Parser, FuncName, FuncValue->Val->FuncDef.Intrinsic? FuncValue->Val->FuncDef.NumParams: 0);
         Parser->pc->TopStackFrame->NumParams = ArgCount;
         Parser->pc->TopStackFrame->ReturnValue = ReturnValue;
      // Function parameters should not go out of scope.
         Parser->ScopeID = -1;
         for (int Count = 0; Count < FuncValue->Val->FuncDef.NumParams; Count++)
            VariableDefine(Parser->pc, Parser, FuncValue->Val->FuncDef.ParamName[Count], ParamArray[Count], NULL, true);
         Parser->ScopeID = OldScopeID;
         if (ParseStatement(&FuncParser, true) != OkSyn)
            ProgramFail(&FuncParser, "function body expected");
         if (RunIt) {
            if (FuncParser.Mode == RunM && FuncValue->Val->FuncDef.ReturnType != &Parser->pc->VoidType)
               ProgramFail(&FuncParser, "no value returned from a function returning %t", FuncValue->Val->FuncDef.ReturnType);
            else if (FuncParser.Mode == GotoM)
               ProgramFail(&FuncParser, "couldn't find goto label '%s'", FuncParser.SearchGotoLabel);
         }
         VariableStackFramePop(Parser);
      } else
         FuncValue->Val->FuncDef.Intrinsic(Parser, ReturnValue, ParamArray, ArgCount);
      HeapPopStackFrame(Parser->pc);
   }
   Parser->Mode = OldMode;
}

// Parse an expression.
long ExpressionParseInt(ParseState Parser) {
   Value Val;
   if (!ExpressionParse(Parser, &Val))
      ProgramFail(Parser, "expression expected");
   long Result = 0;
   if (Parser->Mode == RunM) {
      if (!IsNumVal(Val))
         ProgramFail(Parser, "integer value expected instead of %t", Val->Typ);
      Result = ExpressionCoerceInteger(Val);
      VariableStackPop(Parser, Val);
   }
   return Result;
}
