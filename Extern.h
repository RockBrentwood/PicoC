// picoc main header file:
// This has all the main data structures and function prototypes.
// If you're just calling picoc you should look at the external interface instead, in Main.h.
#ifndef EXTERN_H
#define EXTERN_H

#include "Sys.h"

// Handy definitions.
// Should be replaced by #include <stdbool.h>, and might later be.
typedef enum { false, true } bool;
#ifndef NULL
// Should be replaced by #include <stdlib.h>, and might later be.
#   define NULL ((void *)0)
#endif
#ifndef min
#   define min(x, y) (((x) < (y))? (x): (y))
#endif

#define MEM_ALIGN(x) (((x) + sizeof(ALIGN_TYPE) - 1)&~(sizeof(ALIGN_TYPE) - 1))
#define GETS_BUF_MAX 256

// For debugging.
#define PRINT_SOURCE_POS ({ PrintSourceTextErrorLine(Parser->pc->CStdOut, Parser->FileName, Parser->SourceText, Parser->Line, Parser->CharacterPos); PlatformPrintf(Parser->pc->CStdOut, "\n"); })
#define PRINT_TYPE(typ) PlatformPrintf(Parser->pc->CStdOut, "%t\n", typ);

// Small processors use a simplified FILE * for stdio, otherwise use the system FILE *.
#ifdef BUILTIN_MINI_STDLIB
typedef struct OutputStream OutStruct, *OutFile;
#else
typedef FILE OutStruct, *OutFile;
#endif

// Coercion of numeric types to other numeric types.
#ifndef NO_FP
#   define IS_FP(v) ((v)->Typ->Base == TypeFP)
#   define FP_VAL(v) ((v)->Val->FP)
#else
#   define IS_FP(v) false
#   define FP_VAL(v) 0
#endif

#define IS_POINTER_COERCIBLE(v, ap) ((ap)? ((v)->Typ->Base == TypePointer): false)
#define POINTER_COERCE(v) ((int)(v)->Val->Pointer)

#define IS_INTEGER_NUMERIC_TYPE(t) ((t)->Base >= TypeInt && (t)->Base <= TypeUnsignedLong)
#define IS_INTEGER_NUMERIC(v) IS_INTEGER_NUMERIC_TYPE((v)->Typ)
#define IS_NUMERIC_COERCIBLE(v) (IS_INTEGER_NUMERIC(v) || IS_FP(v))
#define IS_NUMERIC_COERCIBLE_PLUS_POINTERS(v, ap) (IS_NUMERIC_COERCIBLE(v) || IS_POINTER_COERCIBLE(v, ap))

typedef struct Table *Table;
typedef struct State *State;

// Lexical tokens.
typedef enum LexToken {
/*0x00*/ TokenNone,
/*0x01*/ TokenComma,
/*0x02*/ TokenAssign, TokenAddAssign, TokenSubtractAssign, TokenMultiplyAssign, TokenDivideAssign, TokenModulusAssign,
/*0x08*/ TokenShiftLeftAssign, TokenShiftRightAssign, TokenArithmeticAndAssign, TokenArithmeticOrAssign, TokenArithmeticExorAssign,
/*0x0d*/ TokenQuestionMark, TokenColon,
/*0x0f*/ TokenLogicalOr,
/*0x10*/ TokenLogicalAnd,
/*0x11*/ TokenArithmeticOr,
/*0x12*/ TokenArithmeticExor,
/*0x13*/ TokenAmpersand,
/*0x14*/ TokenEqual, TokenNotEqual,
/*0x16*/ TokenLessThan, TokenGreaterThan, TokenLessEqual, TokenGreaterEqual,
/*0x1a*/ TokenShiftLeft, TokenShiftRight,
/*0x1c*/ TokenPlus, TokenMinus,
/*0x1e*/ TokenAsterisk, TokenSlash, TokenModulus,
/*0x21*/ TokenIncrement, TokenDecrement, TokenUnaryNot, TokenUnaryExor, TokenSizeof, TokenCast,
/*0x27*/ TokenLeftSquareBracket, TokenRightSquareBracket, TokenDot, TokenArrow,
/*0x2b*/ TokenOpenBracket, TokenCloseBracket,
/*0x2d*/ TokenIdentifier, TokenIntegerConstant, TokenFPConstant, TokenStringConstant, TokenCharacterConstant,
/*0x32*/ TokenSemicolon, TokenEllipsis,
/*0x34*/ TokenLeftBrace, TokenRightBrace,
/*0x36*/ TokenIntType, TokenCharType, TokenFloatType, TokenDoubleType, TokenVoidType, TokenEnumType,
/*0x3c*/ TokenLongType, TokenSignedType, TokenShortType, TokenStaticType, TokenAutoType, TokenRegisterType, TokenExternType, TokenStructType, TokenUnionType, TokenUnsignedType, TokenTypedef,
/*0x46*/ TokenContinue, TokenDo, TokenElse, TokenFor, TokenGoto, TokenIf, TokenWhile, TokenBreak, TokenSwitch, TokenCase, TokenDefault, TokenReturn,
/*0x52*/ TokenHashDefine, TokenHashInclude, TokenHashIf, TokenHashIfdef, TokenHashIfndef, TokenHashElse, TokenHashEndif,
/*0x59*/ TokenNew, TokenDelete,
/*0x5b*/ TokenOpenMacroBracket,
/*0x5c*/ TokenEOF, TokenEndOfLine, TokenEndOfFunction
} LexToken;

// Used in dynamic memory allocation.
typedef struct AllocNode *AllocNode;
struct AllocNode {
   unsigned int Size;
   AllocNode NextFree;
};

// Whether we're running or skipping code.
typedef enum RunMode {
   RunModeRun, // We're running code as we parse it.
   RunModeSkip, // Skipping code, not running.
   RunModeReturn, // Returning from a function.
   RunModeCaseSearch, // Searching for a case label.
   RunModeBreak, // Breaking out of a switch/while/do.
   RunModeContinue, // As above but repeat the loop.
   RunModeGoto // Searching for a goto label.
} RunMode;

// Parser state - has all this detail so we can parse nested files.
typedef struct ParseState {
   State pc; // The picoc instance this parser is a part of.
   const unsigned char *Pos; // The character position in the source text.
   char *FileName; // What file we're executing (registered string).
   short int Line; // Line number we're executing.
   short int CharacterPos; // Character/column in the line we're executing.
   RunMode Mode; // Whether to skip or run code.
   int SearchLabel; // What case label we're searching for.
   const char *SearchGotoLabel; // What goto label we're searching for.
   const char *SourceText; // The entire source text.
   short int HashIfLevel; // How many "if"s we're nested down.
   short int HashIfEvaluateToLevel; // If we're not evaluating an if branch, what the last evaluated level was.
   bool DebugMode; // Debugging mode.
   int ScopeID; // For keeping track of local variables (free them after they go out of scope).
} *ParseState;

// Values.
typedef enum BaseType {
   TypeVoid, // No type.
   TypeInt, // Integer.
   TypeShort, // Short integer.
   TypeChar, // A single character (signed).
   TypeLong, // Long integer.
   TypeUnsignedInt, // Unsigned integer.
   TypeUnsignedShort, // Unsigned short integer.
   TypeUnsignedChar, // Unsigned 8-bit number: must be before unsigned long.
   TypeUnsignedLong, // Unsigned long integer.
#ifndef NO_FP
   TypeFP, // Floating point.
#endif
   TypeFunction, // A function.
   TypeMacro, // A macro.
   TypePointer, // A pointer.
   TypeArray, // An array of a sub-type.
   TypeStruct, // Aggregate type.
   TypeUnion, // Merged type.
   TypeEnum, // Enumerated integer type.
   TypeGotoLabel, // A label we can "goto".
   Type_Type // A type for storing types.
} BaseType;

// Data type.
typedef struct ValueType *ValueType;
struct ValueType {
   BaseType Base; // What kind of type this is.
   int ArraySize; // The size of an array type.
   int Sizeof; // The storage required.
   int AlignBytes; // The alignment boundary of this type.
   const char *Identifier; // The name of a struct or union.
   ValueType FromType; // The type we're derived from (or NULL).
   ValueType DerivedTypeList; // First in a list of types derived from this one.
   ValueType Next; // Next item in the derived type list.
   Table Members; // Members of a struct or union.
   bool OnHeap; // True if allocated on the heap.
   bool StaticQualifier; // True if it's a static.
};

// Function definition.
struct FuncDef {
   ValueType ReturnType; // The return value type.
   int NumParams; // The number of parameters.
   int VarArgs; // Has a variable number of arguments after the explicitly specified ones.
   ValueType *ParamType; // Array of parameter types.
   char **ParamName; // Array of parameter names.
   void (*Intrinsic)(); // Intrinsic call address or NULL.
   struct ParseState Body; // Lexical tokens of the function body if not intrinsic.
};

// Macro definition.
typedef struct MacroDef {
   int NumParams; // The number of parameters.
   char **ParamName; // Array of parameter names.
   struct ParseState Body; // Lexical tokens of the function body if not intrinsic.
} *MacroDef;

// Values.
typedef union AnyValue {
   char Character;
   short ShortInteger;
   int Integer;
   long LongInteger;
   unsigned short UnsignedShortInteger;
   unsigned int UnsignedInteger;
   unsigned long UnsignedLongInteger;
   unsigned char UnsignedCharacter;
   char *Identifier;
   char ArrayMem[2]; // Placeholder for where the data starts, doesn't point to it.
   ValueType Typ;
   struct FuncDef FuncDef;
   struct MacroDef MacroDef;
#ifndef NO_FP
   double FP;
#endif
   void *Pointer; // Unsafe native pointers.
} *AnyValue;

typedef struct Value *Value;
struct Value {
   ValueType Typ; // The type of this value.
   AnyValue Val; // Pointer to the AnyValue which holds the actual content.
   Value LValueFrom; // If an LValue, this is a Value our LValue is contained within (or NULL).
   bool ValOnHeap; // This Value is on the heap.
   bool ValOnStack; // The AnyValue is on the stack along with this Value.
   bool AnyValOnHeap; // The AnyValue is separately allocated from the Value on the heap.
   bool IsLValue; // Is modifiable and is allocated somewhere we can usefully modify it.
   int ScopeID; // To know when it goes out of scope.
   bool OutOfScope;
};

// Hash table data structure.
typedef struct TableEntry *TableEntry;
struct TableEntry {
   TableEntry Next; // Next item in this hash chain.
   const char *DeclFileName; // Where the variable was declared.
   unsigned short DeclLine;
   unsigned short DeclColumn;
   union TableEntryPayload {
      struct ValueEntry {
         char *Key; // Points to the shared string table.
         Value Val; // The value we're storing.
      } v; // Used for tables of values.
      char Key[1]; // Dummy size - used for the shared string table.
      struct BreakpointEntry { // Defines a breakpoint.
         const char *FileName;
         short int Line;
         short int CharacterPos;
      } b;
   } p;
};

struct Table {
   short Size;
   bool OnHeap;
   TableEntry *HashTable;
};

// Stack frame for function calls.
typedef struct StackFrame *StackFrame;
struct StackFrame {
   struct ParseState ReturnParser; // How we got here.
   const char *FuncName; // The name of the function we're in.
   Value ReturnValue; // Copy the return value here.
   Value *Parameter; // Array of parameter values.
   int NumParams; // The number of parameters.
   struct Table LocalTable; // The local variables and parameters.
   TableEntry LocalHashTable[LOCAL_TABLE_SIZE];
   StackFrame PreviousStackFrame; // The next lower stack frame.
};

// Lexer state.
typedef struct LexState {
   const char *Pos;
   const char *End;
   const char *FileName;
   int Line;
   int CharacterPos;
   const char *SourceText;
   enum { LexModeNormal, LexModeHashInclude, LexModeHashDefine, LexModeHashDefineSpace, LexModeHashDefineSpaceIdent } Mode;
   int EmitExtraNewlines;
} *LexState;

// Library function definition.
typedef struct LibraryFunction {
   void (*Func)(ParseState Parser, Value, Value *, int);
   const char *Prototype;
} *LibraryFunction;

// Output stream-type specific state information.
typedef struct OutputStreamInfo {
   ParseState Parser;
   char *WritePos;
} *OutputStreamInfo;

// Used when writing output to a string - e.g. sprintf().
typedef struct OutputStream {
// Stream-specific method for writing characters to the console.
   void (*Putch)(unsigned char, OutputStreamInfo);
   struct OutputStreamInfo StrI;
} *OutputStream;

// Possible results of parsing a statement.
typedef enum ParseResult { ParseResultEOF, ParseResultError, ParseResultOk } ParseResult;

// A chunk of heap-allocated tokens we'll cleanup when we're done.
typedef struct CleanupTokenNode *CleanupTokenNode;
struct CleanupTokenNode {
   void *Tokens;
   const char *SourceText;
   CleanupTokenNode Next;
};

// Linked list of lexical tokens used in interactive mode.
typedef struct TokenLine *TokenLine;
struct TokenLine {
   TokenLine Next;
   unsigned char *Tokens;
   int NumBytes;
};

// A list of libraries we can include.
typedef struct IncludeLibrary *IncludeLibrary;
struct IncludeLibrary {
   char *IncludeName;
   void (*SetupFunction)(State pc);
   LibraryFunction FuncList;
   const char *SetupCSource;
   IncludeLibrary NextLib;
};

#define FREELIST_BUCKETS 8 // Freelists for 4, 8, 12 ... 32 byte allocs.
#define SPLIT_MEM_THRESHOLD 16 // Don't split memory which is close in size.
#define BREAKPOINT_TABLE_SIZE 21

// The entire state of the picoc system.
struct State {
// Parser global data.
   struct Table GlobalTable;
   CleanupTokenNode CleanupTokenList;
   TableEntry GlobalHashTable[GLOBAL_TABLE_SIZE];
// Lexer global data.
   TokenLine InteractiveHead;
   TokenLine InteractiveTail;
   TokenLine InteractiveCurrentLine;
   bool LexUseStatementPrompt;
   union AnyValue LexAnyValue;
   struct Value LexValue;
   struct Table ReservedWordTable;
   TableEntry ReservedWordHashTable[RESERVED_WORD_TABLE_SIZE];
// The table of string literal values.
   struct Table StringLiteralTable;
   TableEntry StringLiteralHashTable[STRING_LITERAL_TABLE_SIZE];
// The stack.
   StackFrame TopStackFrame;
// The value passed to exit().
   int PicocExitValue;
// A list of libraries we can include.
   IncludeLibrary IncludeLibList;
// Heap memory.
#ifdef USE_MALLOC_STACK
   unsigned char *HeapMemory; // Stack memory since our heap is malloc()ed.
   void *HeapBottom; // The bottom of the (downward-growing) heap.
   void *StackFrame; // The current stack frame.
   void *HeapStackTop; // The top of the stack.
#else
#ifdef SURVEYOR_HOST
   unsigned char *HeapMemory; // All memory - stack and heap.
   void *HeapBottom; // The bottom of the (downward-growing) heap.
   void *StackFrame; // The current stack frame.
   void *HeapStackTop; // The top of the stack.
   void *HeapMemStart;
#else
   unsigned char HeapMemory[HEAP_SIZE]; // All memory - stack and heap.
   void *HeapBottom; // The bottom of the (downward-growing) heap.
   void *StackFrame; // The current stack frame.
   void *HeapStackTop; // The top of the stack.
#endif
#endif
   AllocNode FreeListBucket[FREELIST_BUCKETS]; // We keep a pool of freelist buckets to reduce fragmentation.
   AllocNode FreeListBig; // Free memory which doesn't fit in a bucket.
// Types.
   struct ValueType UberType;
   struct ValueType IntType;
   struct ValueType ShortType;
   struct ValueType CharType;
   struct ValueType LongType;
   struct ValueType UnsignedIntType;
   struct ValueType UnsignedShortType;
   struct ValueType UnsignedLongType;
   struct ValueType UnsignedCharType;
#ifndef NO_FP
   struct ValueType FPType;
#endif
   struct ValueType VoidType;
   struct ValueType TypeType;
   struct ValueType FunctionType;
   struct ValueType MacroType;
   struct ValueType EnumType;
   struct ValueType GotoLabelType;
   ValueType CharPtrType;
   ValueType CharPtrPtrType;
   ValueType CharArrayType;
   ValueType VoidPtrType;
// Debugger.
   struct Table BreakpointTable;
   TableEntry BreakpointHashTable[BREAKPOINT_TABLE_SIZE];
   int BreakpointCount;
   int DebugManualBreak;
// C library.
   int BigEndian;
   int LittleEndian;
   OutFile CStdOut;
   OutStruct CStdOutBase;
// The picoc version string.
   const char *VersionString;
// Exit longjump buffer.
#if defined(UNIX_HOST) || defined(WIN32)
   jmp_buf PicocExitBuf;
#endif
#ifdef SURVEYOR_HOST
   int PicocExitBuf[41];
#endif
// String table.
   struct Table StringTable;
   TableEntry StringHashTable[STRING_TABLE_SIZE];
   char *StrEmpty;
};

// Table.c:
void TableInit(State pc);
void TableInitTable(Table Tbl, TableEntry *HashTable, int Size, bool OnHeap);
bool TableSet(State pc, Table Tbl, char *Key, Value Val, const char *DeclFileName, int DeclLine, int DeclColumn);
bool TableGet(Table Tbl, const char *Key, Value *Val, const char **DeclFileName, int *DeclLine, int *DeclColumn);
Value TableDelete(State pc, Table Tbl, const char *Key);
char *TableStrRegister2(State pc, const char *Str, int Len);
char *TableStrRegister(State pc, const char *Str);
void TableStrFree(State pc);

// Lex.c:
void LexInit(State pc);
void LexCleanup(State pc);
void *LexAnalyse(State pc, const char *FileName, const char *Source, int SourceLen, int *TokenLen);
void LexInitParser(ParseState Parser, State pc, const char *SourceText, void *TokenSource, char *FileName, bool RunIt, bool EnableDebugger);
LexToken LexGetToken(ParseState Parser, Value *ValP, int IncPos);
LexToken LexRawPeekToken(ParseState Parser);
void LexToEndOfLine(ParseState Parser);
void *LexCopyTokens(ParseState StartParser, ParseState EndParser);
void LexInteractiveClear(State pc, ParseState Parser);
void LexInteractiveCompleted(State pc, ParseState Parser);
void LexInteractiveStatementPrompt(State pc);

// Syn.c:
#if 0
// The following are declared in Main.h:
void PicocParse(State pc, const char *FileName, const char *Source, int SourceLen, bool RunIt, bool CleanupNow, bool CleanupSource, bool EnableDebugger);
void PicocParseInteractive(State pc);
#endif
void ParseCleanup(State pc);
Value ParseFunctionDefinition(ParseState Parser, ValueType ReturnType, char *Identifier);
void ParserCopy(ParseState To, ParseState From);
ParseResult ParseStatement(ParseState Parser, bool CheckTrailingSemicolon);
void PicocParseInteractiveNoStartPrompt(State pc, bool EnableDebugger);

// Exp.c:
long ExpressionCoerceInteger(Value Val);
unsigned long ExpressionCoerceUnsignedInteger(Value Val);
#ifndef NO_FP
double ExpressionCoerceFP(Value Val);
#endif
void ExpressionAssign(ParseState Parser, Value DestValue, Value SourceValue, bool Force, const char *FuncName, int ParamNo, bool AllowPointerCoercion);
bool ExpressionParse(ParseState Parser, Value *Result);
long ExpressionParseInt(ParseState Parser);

// Type.c:
void TypeInit(State pc);
void TypeCleanup(State pc);
int TypeSize(ValueType Typ, int ArraySize, bool Compact);
int TypeSizeValue(Value Val, bool Compact);
int TypeStackSizeValue(Value Val);
#if 0
// Isn't defined anywhere.
int TypeLastAccessibleOffset(State pc, Value Val);
#endif
bool TypeParseFront(ParseState Parser, ValueType *Typ, bool *IsStatic);
void TypeParseIdentPart(ParseState Parser, ValueType BasicTyp, ValueType *Typ, char **Identifier);
void TypeParse(ParseState Parser, ValueType *Typ, char **Identifier, bool *IsStatic);
ValueType TypeGetMatching(State pc, ParseState Parser, ValueType ParentType, BaseType Base, int ArraySize, const char *Identifier, bool AllowDuplicates);
ValueType TypeCreateOpaqueStruct(State pc, ParseState Parser, const char *StructName, int Size);
bool TypeIsForwardDeclared(ParseState Parser, ValueType Typ);

// Heap.c:
void HeapInit(State pc, int StackOrHeapSize);
void HeapCleanup(State pc);
void *HeapAllocStack(State pc, int Size);
void HeapUnpopStack(State pc, int Size);
bool HeapPopStack(State pc, void *Addr, int Size);
void HeapPushStackFrame(State pc);
bool HeapPopStackFrame(State pc);
void *HeapAllocMem(State pc, int Size);
void HeapFreeMem(State pc, void *Mem);

// Var.c:
void VariableInit(State pc);
void VariableFree(State pc, Value Val);
void VariableTableCleanup(State pc, Table HashTable);
void VariableCleanup(State pc);
void *VariableAlloc(State pc, ParseState Parser, int Size, bool OnHeap);
Value VariableAllocValueAndData(State pc, ParseState Parser, int DataSize, bool IsLValue, Value LValueFrom, bool OnHeap);
Value VariableAllocValueFromType(State pc, ParseState Parser, ValueType Typ, bool IsLValue, Value LValueFrom, bool OnHeap);
Value VariableAllocValueAndCopy(State pc, ParseState Parser, Value FromValue, bool OnHeap);
Value VariableAllocValueFromExistingData(ParseState Parser, ValueType Typ, AnyValue FromValue, bool IsLValue, Value LValueFrom);
Value VariableAllocValueShared(ParseState Parser, Value FromValue);
void VariableRealloc(ParseState Parser, Value FromValue, int NewSize);
int VariableScopeBegin(ParseState Parser, int *OldScopeID);
void VariableScopeEnd(ParseState Parser, int ScopeID, int PrevScopeID);
bool VariableDefinedAndOutOfScope(State pc, const char *Ident);
Value VariableDefine(State pc, ParseState Parser, char *Ident, Value InitValue, ValueType Typ, bool MakeWritable);
Value VariableDefineButIgnoreIdentical(ParseState Parser, char *Ident, ValueType Typ, bool IsStatic, bool *FirstVisit);
bool VariableDefined(State pc, const char *Ident);
void VariableGet(State pc, ParseState Parser, const char *Ident, Value *LVal);
void VariableDefinePlatformVar(State pc, ParseState Parser, char *Ident, ValueType Typ, AnyValue FromValue, bool IsWritable);
void VariableStackPop(ParseState Parser, Value Var);
void VariableStackFrameAdd(ParseState Parser, const char *FuncName, int NumParams);
void VariableStackFramePop(ParseState Parser);
Value VariableStringLiteralGet(State pc, char *Ident);
void VariableStringLiteralDefine(State pc, char *Ident, Value Val);
void *VariableDereferencePointer(ParseState Parser, Value PointerValue, Value *DerefVal, int *DerefOffset, ValueType *DerefType, bool *DerefIsLValue);

// Lib.c:
void LibraryInit(State pc);
void LibraryAdd(State pc, Table GlobalTable, const char *LibraryName, LibraryFunction FuncList);
void PrintType(ValueType Typ, OutFile Stream);
void BasicIOInit(State pc);
void CLibraryInit(State pc);
void PrintCh(char OutCh, OutFile Stream);
void PrintStr(const char *Str, OutFile Stream);
void PrintSimpleInt(long Num, OutFile Stream);
void PrintFP(double Num, OutFile Stream);
void LibPrintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs);
// May be defined in Lib.c or in Lib/stdio.c:
// BasicIOInit
// PrintCh
// PrintStr
// PrintSimpleInt
// PrintFP

// Sys.c:
#if 0
// The following are declared in Main.h:
void PicocInitialize(State pc, int StackSize);
void PicocCleanup(State pc);
void PicocCallMain(State pc, int argc, char **argv);
// Defined in the following places:
// PicocPlatformSetExitPoint	Main.h as a macro.
// PicocPlatformScanFile	Sys/Sys{UNIX,MSVC,FFox}.c
int PicocPlatformSetExitPoint(State pc);
void PicocPlatformScanFile(State pc, const char *FileName);
#endif
void PrintSourceTextErrorLine(OutFile Stream, const char *FileName, const char *SourceText, int Line, int CharacterPos);
void ProgramFail(ParseState Parser, const char *Message, ...);
void ProgramFailNoParser(State pc, const char *Message, ...);
void AssignFail(ParseState Parser, const char *Format, ValueType Type1, ValueType Type2, int Num1, int Num2, const char *FuncName, int ParamNo);
void LexFail(State pc, LexState Lexer, const char *Message, ...);
void PlatformPrintf(OutFile Stream, const char *Format, ...);
char *PlatformMakeTempName(State pc, char *TempNameBuffer);
// Defined in the following places:
// PlatformInit		Sys/Sys{UNIX,MSVC}.c
// PlatformCleanup	Sys/Sys{UNIX,MSVC,FFox,Surveyor}.c
// PlatformGetLine	Sys/Sys{UNIX,MSVC,FFox,Surveyor}.c
// PlatformGetCharacter	Sys/Sys{UNIX,MSVC,FFox,Surveyor}.c
// PlatformPutc		Sys/Sys{UNIX,MSVC,FFox,Surveyor}.c
// PlatformExit		Sys/Sys{UNIX,MSVC,FFox,Surveyor}.c
// PlatformLibraryInit	Sys/Lib{UNIX,MSVC,FFox,Surveyor,Srv1}.c
void PlatformInit(State pc);
void PlatformCleanup(State pc);
char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt);
int PlatformGetCharacter();
void PlatformPutc(unsigned char OutCh, OutputStreamInfo Stream);
void PlatformExit(State pc, int RetVal);
void PlatformLibraryInit(State pc);

// Inc.c:
#if 0
// The following is declared in Main.h:
void PicocIncludeAllSystemHeaders(State pc);
#endif
void IncludeInit(State pc);
void IncludeCleanup(State pc);
void IncludeRegister(State pc, const char *IncludeName, void (*SetupFunction)(State pc), LibraryFunction FuncList, const char *SetupCSource);
void IncludeFile(State pc, char *FileName);

#ifndef NO_DEBUGGER
// Debug.c:
void DebugInit(State pc);
void DebugCleanup(State pc);
#if 0
DebugSetBreakpoint(ParseState Parser);
DebugClearBreakpoint(ParseState Parser);
void DebugStep();
#endif
void DebugCheckStatement(ParseState Parser);
#endif

// Lib/stdio.c:
extern const char StdioDefs[];
extern struct LibraryFunction StdioFunctions[];
void StdioSetupFunc(State pc);

// Lib/math.c:
extern struct LibraryFunction MathFunctions[];
void MathSetupFunc(State pc);

// Lib/string.c:
extern struct LibraryFunction StringFunctions[];
void StringSetupFunc(State pc);

// Lib/stdlib.c:
extern struct LibraryFunction StdlibFunctions[];
void StdlibSetupFunc(State pc);

// Lib/time.c:
extern const char StdTimeDefs[];
extern struct LibraryFunction StdTimeFunctions[];
void StdTimeSetupFunc(State pc);

// Lib/errno.c:
void StdErrnoSetupFunc(State pc);

// Lib/ctype.c:
extern struct LibraryFunction StdCtypeFunctions[];

// Lib/stdbool.c:
extern const char StdboolDefs[];
void StdboolSetupFunc(State pc);

// Lib/unistd.c:
extern const char UnistdDefs[];
extern struct LibraryFunction UnistdFunctions[];
void UnistdSetupFunc(State pc);

#endif // EXTERN_H.
