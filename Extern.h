// picoc main header file:
// This has all the main data structures and function prototypes.
// If you're just calling picoc you should look at the external interface instead, in Main.h.
#ifndef EXTERN_H
#define EXTERN_H

#include "Sys.h"

// Handy definitions.
#ifndef TRUE
#   define TRUE 1
#   define FALSE 0
#endif
#ifndef NULL
#   define NULL 0
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
typedef struct OutputStream IOFILE;
#else
typedef FILE IOFILE;
#endif

// Coercion of numeric types to other numeric types.
#ifndef NO_FP
#   define IS_FP(v) ((v)->Typ->Base == TypeFP)
#   define FP_VAL(v) ((v)->Val->FP)
#else
#   define IS_FP(v) 0
#   define FP_VAL(v) 0
#endif

#define IS_POINTER_COERCIBLE(v, ap) ((ap)? ((v)->Typ->Base == TypePointer): 0)
#define POINTER_COERCE(v) ((int)(v)->Val->Pointer)

#define IS_INTEGER_NUMERIC_TYPE(t) ((t)->Base >= TypeInt && (t)->Base <= TypeUnsignedLong)
#define IS_INTEGER_NUMERIC(v) IS_INTEGER_NUMERIC_TYPE((v)->Typ)
#define IS_NUMERIC_COERCIBLE(v) (IS_INTEGER_NUMERIC(v) || IS_FP(v))
#define IS_NUMERIC_COERCIBLE_PLUS_POINTERS(v, ap) (IS_NUMERIC_COERCIBLE(v) || IS_POINTER_COERCIBLE(v, ap))

struct Table;
struct Picoc_Struct;
typedef struct Picoc_Struct Picoc;

// Lexical tokens.
enum LexToken {
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
};

// Used in dynamic memory allocation.
struct AllocNode {
   unsigned int Size;
   struct AllocNode *NextFree;
};

// Whether we're running or skipping code.
enum RunMode {
   RunModeRun, // We're running code as we parse it.
   RunModeSkip, // Skipping code, not running.
   RunModeReturn, // Returning from a function.
   RunModeCaseSearch, // Searching for a case label.
   RunModeBreak, // Breaking out of a switch/while/do.
   RunModeContinue, // As above but repeat the loop.
   RunModeGoto // Searching for a goto label.
};

// Parser state - has all this detail so we can parse nested files.
struct ParseState {
   Picoc *pc; // The picoc instance this parser is a part of.
   const unsigned char *Pos; // The character position in the source text.
   char *FileName; // What file we're executing (registered string).
   short int Line; // Line number we're executing.
   short int CharacterPos; // Character/column in the line we're executing.
   enum RunMode Mode; // Whether to skip or run code.
   int SearchLabel; // What case label we're searching for.
   const char *SearchGotoLabel; // What goto label we're searching for.
   const char *SourceText; // The entire source text.
   short int HashIfLevel; // How many "if"s we're nested down.
   short int HashIfEvaluateToLevel; // If we're not evaluating an if branch, what the last evaluated level was.
   char DebugMode; // Debugging mode.
   int ScopeID; // For keeping track of local variables (free them after they go out of scope).
};

// Values.
enum BaseType {
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
};

// Data type.
struct ValueType {
   enum BaseType Base; // What kind of type this is.
   int ArraySize; // The size of an array type.
   int Sizeof; // The storage required.
   int AlignBytes; // The alignment boundary of this type.
   const char *Identifier; // The name of a struct or union.
   struct ValueType *FromType; // The type we're derived from (or NULL).
   struct ValueType *DerivedTypeList; // First in a list of types derived from this one.
   struct ValueType *Next; // Next item in the derived type list.
   struct Table *Members; // Members of a struct or union.
   int OnHeap; // True if allocated on the heap.
   int StaticQualifier; // True if it's a static.
};

// Function definition.
struct FuncDef {
   struct ValueType *ReturnType; // The return value type.
   int NumParams; // The number of parameters.
   int VarArgs; // Has a variable number of arguments after the explicitly specified ones.
   struct ValueType **ParamType; // Array of parameter types.
   char **ParamName; // Array of parameter names.
   void (*Intrinsic)(); // Intrinsic call address or NULL.
   struct ParseState Body; // Lexical tokens of the function body if not intrinsic.
};

// Macro definition.
struct MacroDef {
   int NumParams; // The number of parameters.
   char **ParamName; // Array of parameter names.
   struct ParseState Body; // Lexical tokens of the function body if not intrinsic.
};

// Values.
union AnyValue {
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
   struct ValueType *Typ;
   struct FuncDef FuncDef;
   struct MacroDef MacroDef;
#ifndef NO_FP
   double FP;
#endif
   void *Pointer; // Unsafe native pointers.
};

struct Value {
   struct ValueType *Typ; // The type of this value.
   union AnyValue *Val; // Pointer to the AnyValue which holds the actual content.
   struct Value *LValueFrom; // If an LValue, this is a Value our LValue is contained within (or NULL).
   char ValOnHeap; // This Value is on the heap.
   char ValOnStack; // The AnyValue is on the stack along with this Value.
   char AnyValOnHeap; // The AnyValue is separately allocated from the Value on the heap.
   char IsLValue; // Is modifiable and is allocated somewhere we can usefully modify it.
   int ScopeID; // To know when it goes out of scope.
   char OutOfScope;
};

// Hash table data structure.
struct TableEntry {
   struct TableEntry *Next; // Next item in this hash chain.
   const char *DeclFileName; // Where the variable was declared.
   unsigned short DeclLine;
   unsigned short DeclColumn;
   union TableEntryPayload {
      struct ValueEntry {
         char *Key; // Points to the shared string table.
         struct Value *Val; // The value we're storing.
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
   short OnHeap;
   struct TableEntry **HashTable;
};

// Stack frame for function calls.
struct StackFrame {
   struct ParseState ReturnParser; // How we got here.
   const char *FuncName; // The name of the function we're in.
   struct Value *ReturnValue; // Copy the return value here.
   struct Value **Parameter; // Array of parameter values.
   int NumParams; // The number of parameters.
   struct Table LocalTable; // The local variables and parameters.
   struct TableEntry *LocalHashTable[LOCAL_TABLE_SIZE];
   struct StackFrame *PreviousStackFrame; // The next lower stack frame.
};

// Lexer state.
enum LexMode {
   LexModeNormal,
   LexModeHashInclude,
   LexModeHashDefine,
   LexModeHashDefineSpace,
   LexModeHashDefineSpaceIdent
};

struct LexState {
   const char *Pos;
   const char *End;
   const char *FileName;
   int Line;
   int CharacterPos;
   const char *SourceText;
   enum LexMode Mode;
   int EmitExtraNewlines;
};

// Library function definition.
struct LibraryFunction {
   void (*Func)(struct ParseState *Parser, struct Value *, struct Value **, int);
   const char *Prototype;
};

// Output stream-type specific state information.
union OutputStreamInfo {
   struct StringOutputStream {
      struct ParseState *Parser;
      char *WritePos;
   } Str;
};

// Stream-specific method for writing characters to the console.
typedef void CharWriter(unsigned char, union OutputStreamInfo *);

// Used when writing output to a string - e.g. sprintf().
struct OutputStream {
   CharWriter *Putch;
   union OutputStreamInfo i;
};

// Possible results of parsing a statement.
enum ParseResult { ParseResultEOF, ParseResultError, ParseResultOk };

// A chunk of heap-allocated tokens we'll cleanup when we're done.
struct CleanupTokenNode {
   void *Tokens;
   const char *SourceText;
   struct CleanupTokenNode *Next;
};

// Linked list of lexical tokens used in interactive mode.
struct TokenLine {
   struct TokenLine *Next;
   unsigned char *Tokens;
   int NumBytes;
};

// A list of libraries we can include.
struct IncludeLibrary {
   char *IncludeName;
   void (*SetupFunction)(Picoc *pc);
   struct LibraryFunction *FuncList;
   const char *SetupCSource;
   struct IncludeLibrary *NextLib;
};

#define FREELIST_BUCKETS 8 // Freelists for 4, 8, 12 ... 32 byte allocs.
#define SPLIT_MEM_THRESHOLD 16 // Don't split memory which is close in size.
#define BREAKPOINT_TABLE_SIZE 21

// The entire state of the picoc system.
struct Picoc_Struct {
// Parser global data.
   struct Table GlobalTable;
   struct CleanupTokenNode *CleanupTokenList;
   struct TableEntry *GlobalHashTable[GLOBAL_TABLE_SIZE];
// Lexer global data.
   struct TokenLine *InteractiveHead;
   struct TokenLine *InteractiveTail;
   struct TokenLine *InteractiveCurrentLine;
   int LexUseStatementPrompt;
   union AnyValue LexAnyValue;
   struct Value LexValue;
   struct Table ReservedWordTable;
   struct TableEntry *ReservedWordHashTable[RESERVED_WORD_TABLE_SIZE];
// The table of string literal values.
   struct Table StringLiteralTable;
   struct TableEntry *StringLiteralHashTable[STRING_LITERAL_TABLE_SIZE];
// The stack.
   struct StackFrame *TopStackFrame;
// The value passed to exit().
   int PicocExitValue;
// A list of libraries we can include.
   struct IncludeLibrary *IncludeLibList;
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
   struct AllocNode *FreeListBucket[FREELIST_BUCKETS]; // We keep a pool of freelist buckets to reduce fragmentation.
   struct AllocNode *FreeListBig; // Free memory which doesn't fit in a bucket.
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
   struct ValueType *CharPtrType;
   struct ValueType *CharPtrPtrType;
   struct ValueType *CharArrayType;
   struct ValueType *VoidPtrType;
// Debugger.
   struct Table BreakpointTable;
   struct TableEntry *BreakpointHashTable[BREAKPOINT_TABLE_SIZE];
   int BreakpointCount;
   int DebugManualBreak;
// C library.
   int BigEndian;
   int LittleEndian;
   IOFILE *CStdOut;
   IOFILE CStdOutBase;
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
   struct TableEntry *StringHashTable[STRING_TABLE_SIZE];
   char *StrEmpty;
};

// Table.c:
void TableInit(Picoc *pc);
char *TableStrRegister(Picoc *pc, const char *Str);
char *TableStrRegister2(Picoc *pc, const char *Str, int Len);
void TableInitTable(struct Table *Tbl, struct TableEntry **HashTable, int Size, int OnHeap);
int TableSet(Picoc *pc, struct Table *Tbl, char *Key, struct Value *Val, const char *DeclFileName, int DeclLine, int DeclColumn);
int TableGet(struct Table *Tbl, const char *Key, struct Value **Val, const char **DeclFileName, int *DeclLine, int *DeclColumn);
struct Value *TableDelete(Picoc *pc, struct Table *Tbl, const char *Key);
char *TableSetIdentifier(Picoc *pc, struct Table *Tbl, const char *Ident, int IdentLen);
void TableStrFree(Picoc *pc);

// Lex.c:
void LexInit(Picoc *pc);
void LexCleanup(Picoc *pc);
void *LexAnalyse(Picoc *pc, const char *FileName, const char *Source, int SourceLen, int *TokenLen);
void LexInitParser(struct ParseState *Parser, Picoc *pc, const char *SourceText, void *TokenSource, char *FileName, int RunIt, int SetDebugMode);
enum LexToken LexGetToken(struct ParseState *Parser, struct Value **Value, int IncPos);
enum LexToken LexRawPeekToken(struct ParseState *Parser);
void LexToEndOfLine(struct ParseState *Parser);
void *LexCopyTokens(struct ParseState *StartParser, struct ParseState *EndParser);
void LexInteractiveClear(Picoc *pc, struct ParseState *Parser);
void LexInteractiveCompleted(Picoc *pc, struct ParseState *Parser);
void LexInteractiveStatementPrompt(Picoc *pc);

// Syn.c:
#if 0
// The following are defined in Main.h:
void PicocParse(const char *FileName, const char *Source, int SourceLen, int RunIt, int CleanupNow, int CleanupSource);
void PicocParseInteractive();
#endif
void PicocParseInteractiveNoStartPrompt(Picoc *pc, int EnableDebugger);
enum ParseResult ParseStatement(struct ParseState *Parser, int CheckTrailingSemicolon);
struct Value *ParseFunctionDefinition(struct ParseState *Parser, struct ValueType *ReturnType, char *Identifier);
void ParseCleanup(Picoc *pc);
void ParserCopyPos(struct ParseState *To, struct ParseState *From);
void ParserCopy(struct ParseState *To, struct ParseState *From);

// Exp.c:
int ExpressionParse(struct ParseState *Parser, struct Value **Result);
long ExpressionParseInt(struct ParseState *Parser);
void ExpressionAssign(struct ParseState *Parser, struct Value *DestValue, struct Value *SourceValue, int Force, const char *FuncName, int ParamNo, int AllowPointerCoercion);
long ExpressionCoerceInteger(struct Value *Val);
unsigned long ExpressionCoerceUnsignedInteger(struct Value *Val);
#ifndef NO_FP
double ExpressionCoerceFP(struct Value *Val);
#endif

// Type.c:
void TypeInit(Picoc *pc);
void TypeCleanup(Picoc *pc);
int TypeSize(struct ValueType *Typ, int ArraySize, int Compact);
int TypeSizeValue(struct Value *Val, int Compact);
int TypeStackSizeValue(struct Value *Val);
int TypeLastAccessibleOffset(Picoc *pc, struct Value *Val);
int TypeParseFront(struct ParseState *Parser, struct ValueType **Typ, int *IsStatic);
void TypeParseIdentPart(struct ParseState *Parser, struct ValueType *BasicTyp, struct ValueType **Typ, char **Identifier);
void TypeParse(struct ParseState *Parser, struct ValueType **Typ, char **Identifier, int *IsStatic);
struct ValueType *TypeGetMatching(Picoc *pc, struct ParseState *Parser, struct ValueType *ParentType, enum BaseType Base, int ArraySize, const char *Identifier, int AllowDuplicates);
struct ValueType *TypeCreateOpaqueStruct(Picoc *pc, struct ParseState *Parser, const char *StructName, int Size);
int TypeIsForwardDeclared(struct ParseState *Parser, struct ValueType *Typ);

// Heap.c:
void HeapInit(Picoc *pc, int StackSize);
void HeapCleanup(Picoc *pc);
void *HeapAllocStack(Picoc *pc, int Size);
int HeapPopStack(Picoc *pc, void *Addr, int Size);
void HeapUnpopStack(Picoc *pc, int Size);
void HeapPushStackFrame(Picoc *pc);
int HeapPopStackFrame(Picoc *pc);
void *HeapAllocMem(Picoc *pc, int Size);
void HeapFreeMem(Picoc *pc, void *Mem);

// Var.c:
void VariableInit(Picoc *pc);
void VariableCleanup(Picoc *pc);
void VariableFree(Picoc *pc, struct Value *Val);
void VariableTableCleanup(Picoc *pc, struct Table *HashTable);
void *VariableAlloc(Picoc *pc, struct ParseState *Parser, int Size, int OnHeap);
void VariableStackPop(struct ParseState *Parser, struct Value *Var);
struct Value *VariableAllocValueAndData(Picoc *pc, struct ParseState *Parser, int DataSize, int IsLValue, struct Value *LValueFrom, int OnHeap);
struct Value *VariableAllocValueAndCopy(Picoc *pc, struct ParseState *Parser, struct Value *FromValue, int OnHeap);
struct Value *VariableAllocValueFromType(Picoc *pc, struct ParseState *Parser, struct ValueType *Typ, int IsLValue, struct Value *LValueFrom, int OnHeap);
struct Value *VariableAllocValueFromExistingData(struct ParseState *Parser, struct ValueType *Typ, union AnyValue *FromValue, int IsLValue, struct Value *LValueFrom);
struct Value *VariableAllocValueShared(struct ParseState *Parser, struct Value *FromValue);
struct Value *VariableDefine(Picoc *pc, struct ParseState *Parser, char *Ident, struct Value *InitValue, struct ValueType *Typ, int MakeWritable);
struct Value *VariableDefineButIgnoreIdentical(struct ParseState *Parser, char *Ident, struct ValueType *Typ, int IsStatic, int *FirstVisit);
int VariableDefined(Picoc *pc, const char *Ident);
int VariableDefinedAndOutOfScope(Picoc *pc, const char *Ident);
void VariableRealloc(struct ParseState *Parser, struct Value *FromValue, int NewSize);
void VariableGet(Picoc *pc, struct ParseState *Parser, const char *Ident, struct Value **LVal);
void VariableDefinePlatformVar(Picoc *pc, struct ParseState *Parser, char *Ident, struct ValueType *Typ, union AnyValue *FromValue, int IsWritable);
void VariableStackFrameAdd(struct ParseState *Parser, const char *FuncName, int NumParams);
void VariableStackFramePop(struct ParseState *Parser);
struct Value *VariableStringLiteralGet(Picoc *pc, char *Ident);
void VariableStringLiteralDefine(Picoc *pc, char *Ident, struct Value *Val);
void *VariableDereferencePointer(struct ParseState *Parser, struct Value *PointerValue, struct Value **DerefVal, int *DerefOffset, struct ValueType **DerefType, int *DerefIsLValue);
int VariableScopeBegin(struct ParseState *Parser, int *PrevScopeID);
void VariableScopeEnd(struct ParseState *Parser, int ScopeID, int PrevScopeID);

// Lib.c:
void BasicIOInit(Picoc *pc);
void LibraryInit(Picoc *pc);
void LibraryAdd(Picoc *pc, struct Table *GlobalTable, const char *LibraryName, struct LibraryFunction *FuncList);
void CLibraryInit(Picoc *pc);
void PrintCh(char OutCh, IOFILE *Stream);
void PrintSimpleInt(long Num, IOFILE *Stream);
void PrintInt(long Num, int FieldWidth, int ZeroPad, int LeftJustify, IOFILE *Stream);
void PrintStr(const char *Str, IOFILE *Stream);
void PrintFP(double Num, IOFILE *Stream);
void PrintType(struct ValueType *Typ, IOFILE *Stream);
void LibPrintf(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs);

// Sys.c:
#if 0
// The following are defined in Main.h:
void PicocCallMain(int argc, char **argv);
int PicocPlatformSetExitPoint();
void PicocInitialize(int StackSize);
void PicocCleanup();
void PicocPlatformScanFile(const char *FileName);
extern int PicocExitValue;
#endif
void ProgramFail(struct ParseState *Parser, const char *Message, ...);
void ProgramFailNoParser(Picoc *pc, const char *Message, ...);
void AssignFail(struct ParseState *Parser, const char *Format, struct ValueType *Type1, struct ValueType *Type2, int Num1, int Num2, const char *FuncName, int ParamNo);
void LexFail(Picoc *pc, struct LexState *Lexer, const char *Message, ...);
void PlatformInit(Picoc *pc);
void PlatformCleanup(Picoc *pc);
char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt);
int PlatformGetCharacter();
void PlatformPutc(unsigned char OutCh, union OutputStreamInfo *);
void PlatformPrintf(IOFILE *Stream, const char *Format, ...);
void PlatformVPrintf(IOFILE *Stream, const char *Format, va_list Args);
void PlatformExit(Picoc *pc, int ExitVal);
char *PlatformMakeTempName(Picoc *pc, char *TempNameBuffer);
void PlatformLibraryInit(Picoc *pc);

// Inc.c:
void IncludeInit(Picoc *pc);
void IncludeCleanup(Picoc *pc);
void IncludeRegister(Picoc *pc, const char *IncludeName, void (*SetupFunction)(Picoc *pc), struct LibraryFunction *FuncList, const char *SetupCSource);
void IncludeFile(Picoc *pc, char *Filename);
#if 0
// The following is defined in Main.h:
void PicocIncludeAllSystemHeaders();
#endif

// Debug.c:
void DebugInit();
void DebugCleanup();
void DebugCheckStatement(struct ParseState *Parser);

// Lib/stdio.c:
extern const char StdioDefs[];
extern struct LibraryFunction StdioFunctions[];
void StdioSetupFunc(Picoc *pc);

// Lib/math.c:
extern struct LibraryFunction MathFunctions[];
void MathSetupFunc(Picoc *pc);

// Lib/string.c:
extern struct LibraryFunction StringFunctions[];
void StringSetupFunc(Picoc *pc);

// Lib/stdlib.c:
extern struct LibraryFunction StdlibFunctions[];
void StdlibSetupFunc(Picoc *pc);

// Lib/time.c:
extern const char StdTimeDefs[];
extern struct LibraryFunction StdTimeFunctions[];
void StdTimeSetupFunc(Picoc *pc);

// Lib/errno.c:
void StdErrnoSetupFunc(Picoc *pc);

// Lib/ctype.c:
extern struct LibraryFunction StdCtypeFunctions[];

// Lib/stdbool.c:
extern const char StdboolDefs[];
void StdboolSetupFunc(Picoc *pc);

// Lib/unistd.c:
extern const char UnistdDefs[];
extern struct LibraryFunction UnistdFunctions[];
void UnistdSetupFunc(Picoc *pc);

#endif // EXTERN_H.
