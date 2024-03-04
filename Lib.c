// picoc mini standard C library:
// Provides an optional tiny C standard library if BUILTIN_MINI_STDLIB is defined.
#include "Main.h"
#include "Extern.h"

// Endian-ness checking.
static const int __ENDIAN_CHECK__ = 1;
static int BigEndian;
static int LittleEndian;

// Global initialisation for libraries.
void LibraryInit(State pc) {
// Define the version number macro.
   pc->VersionString = TableStrRegister(pc, PICOC_VERSION);
   VariableDefinePlatformVar(pc, NULL, "PICOC_VERSION", pc->CharPtrType, (AnyValue)&pc->VersionString, false);
// Define endian-ness macros.
   BigEndian = ((*(char *)&__ENDIAN_CHECK__) == 0);
   LittleEndian = ((*(char *)&__ENDIAN_CHECK__) == 1);
   VariableDefinePlatformVar(pc, NULL, "BIG_ENDIAN", &pc->IntType, (AnyValue)&BigEndian, false);
   VariableDefinePlatformVar(pc, NULL, "LITTLE_ENDIAN", &pc->IntType, (AnyValue)&LittleEndian, false);
}

// Add a library.
void LibraryAdd(State pc, Table GlobalTable, const char *LibraryName, LibraryFunction FuncList) {
   struct ParseState Parser;
   int Count;
   char *Identifier;
   ValueType ReturnType;
   Value NewValue;
   void *Tokens;
   char *IntrinsicName = TableStrRegister(pc, "c library");
// Read all the library definitions.
   for (Count = 0; FuncList[Count].Prototype != NULL; Count++) {
      Tokens = LexAnalyse(pc, IntrinsicName, FuncList[Count].Prototype, strlen((char *)FuncList[Count].Prototype), NULL);
      LexInitParser(&Parser, pc, FuncList[Count].Prototype, Tokens, IntrinsicName, true, false);
      TypeParse(&Parser, &ReturnType, &Identifier, NULL);
      NewValue = ParseFunctionDefinition(&Parser, ReturnType, Identifier);
      NewValue->Val->FuncDef.Intrinsic = FuncList[Count].Func;
      HeapFreeMem(pc, Tokens);
   }
}

// Print a type to a stream without using printf/sprintf.
void PrintType(ValueType Typ, OutFile Stream) {
   switch (Typ->Base) {
      case TypeVoid:
         PrintStr("void", Stream);
      break;
      case TypeInt:
         PrintStr("int", Stream);
      break;
      case TypeShort:
         PrintStr("short", Stream);
      break;
      case TypeChar:
         PrintStr("char", Stream);
      break;
      case TypeLong:
         PrintStr("long", Stream);
      break;
      case TypeUnsignedInt:
         PrintStr("unsigned int", Stream);
      break;
      case TypeUnsignedShort:
         PrintStr("unsigned short", Stream);
      break;
      case TypeUnsignedLong:
         PrintStr("unsigned long", Stream);
      break;
      case TypeUnsignedChar:
         PrintStr("unsigned char", Stream);
      break;
#ifndef NO_FP
      case TypeFP:
         PrintStr("double", Stream);
      break;
#endif
      case TypeFunction:
         PrintStr("function", Stream);
      break;
      case TypeMacro:
         PrintStr("macro", Stream);
      break;
      case TypePointer:
         if (Typ->FromType) PrintType(Typ->FromType, Stream);
         PrintCh('*', Stream);
      break;
      case TypeArray:
         PrintType(Typ->FromType, Stream);
         PrintCh('[', Stream);
         if (Typ->ArraySize != 0) PrintSimpleInt(Typ->ArraySize, Stream);
         PrintCh(']', Stream);
      break;
      case TypeStruct:
         PrintStr("struct ", Stream);
         PrintStr(Typ->Identifier, Stream);
      break;
      case TypeUnion:
         PrintStr("union ", Stream);
         PrintStr(Typ->Identifier, Stream);
      break;
      case TypeEnum:
         PrintStr("enum ", Stream);
         PrintStr(Typ->Identifier, Stream);
      break;
      case TypeGotoLabel:
         PrintStr("goto label ", Stream);
      break;
      case Type_Type:
         PrintStr("type ", Stream);
      break;
   }
}

#ifdef BUILTIN_MINI_STDLIB
// This is a simplified standard library for small embedded systems.
// It doesn't require a system stdio library to operate.

// A more complete standard library for larger computers is in the Sys/Lib*.c files.
static int TRUEValue = 1;
static int ZeroValue = 0;

void BasicIOInit(State pc) {
   pc->CStdOutBase.Putch = &PlatformPutc;
   pc->CStdOut = &CStdOutBase;
}

// Initialize the C library.
void CLibraryInit(State pc) {
// Define some constants.
   VariableDefinePlatformVar(pc, NULL, "NULL", &IntType, (AnyValue)&ZeroValue, false);
   VariableDefinePlatformVar(pc, NULL, "true", &IntType, (AnyValue)&TRUEValue, false);
   VariableDefinePlatformVar(pc, NULL, "false", &IntType, (AnyValue)&ZeroValue, false);
}

// Stream for writing into strings.
static void SPutc(unsigned char Ch, OutputStreamInfo Stream) {
   *Stream->WritePos++ = Ch;
}

// Print a character to a stream without using printf/sprintf.
void PrintCh(char OutCh, OutputStream Stream) {
   (*Stream->Putch)(OutCh, &Stream->i);
}

// Print a string to a stream without using printf/sprintf.
void PrintStr(const char *Str, OutputStream Stream) {
   while (*Str != 0)
      PrintCh(*Str++, Stream);
}

// Print a single character a given number of times.
static void PrintRepeatedChar(State pc, char ShowChar, int Length, OutputStream Stream) {
   while (Length-- > 0)
      PrintCh(ShowChar, Stream);
}

// Print an unsigned integer to a stream without using printf/sprintf.
static void PrintUnsigned(unsigned long Num, unsigned int Base, int FieldWidth, bool ZeroPad, bool LeftJustify, OutputStream Stream) {
   char Result[33];
   int ResPos = sizeof Result;
   Result[--ResPos] = '\0';
   if (Num == 0)
      Result[--ResPos] = '0';
   while (Num > 0) {
      unsigned long NextNum = Num/Base;
      unsigned long Digit = Num - NextNum*Base;
      if (Digit < 10)
         Result[--ResPos] = '0' + Digit;
      else
         Result[--ResPos] = 'a' + Digit - 10;
      Num = NextNum;
   }
   if (FieldWidth > 0 && !LeftJustify)
      PrintRepeatedChar(ZeroPad? '0': ' ', FieldWidth - (sizeof Result - 1 - ResPos), Stream);
   PrintStr(&Result[ResPos], Stream);
   if (FieldWidth > 0 && LeftJustify)
      PrintRepeatedChar(' ', FieldWidth - (sizeof Result - 1 - ResPos), Stream);
}

// Print an integer to a stream without using printf/sprintf.
static void PrintInt(long Num, int FieldWidth, bool ZeroPad, bool LeftJustify, OutputStream Stream) {
   if (Num < 0) {
      PrintCh('-', Stream);
      Num = -Num;
      if (FieldWidth != 0)
         FieldWidth--;
   }
   PrintUnsigned((unsigned long)Num, 10, FieldWidth, ZeroPad, LeftJustify, Stream);
}

// Print an integer to a stream without using printf/sprintf.
void PrintSimpleInt(long Num, OutputStream Stream) {
   PrintInt(Num, -1, false, false, Stream);
}

#ifndef NO_FP
// Print a double to a stream without using printf/sprintf.
void PrintFP(double Num, OutputStream Stream) {
   int Exponent = 0;
   int MaxDecimal;
   if (Num < 0) {
      PrintCh('-', Stream);
      Num = -Num;
   }
   if (Num >= 1e7)
      Exponent = log10(Num);
   else if (Num <= 1e-7 && Num != 0.0)
      Exponent = log10(Num) - 0.999999999;
   Num /= pow(10.0, Exponent);
   PrintInt((long)Num, 0, false, false, Stream);
   PrintCh('.', Stream);
   Num = (Num - (long)Num)*10;
   if (abs(Num) >= 1e-7) {
      for (MaxDecimal = 6; MaxDecimal > 0 && abs(Num) >= 1e-7; Num = (Num - (long)(Num + 1e-7))*10, MaxDecimal--)
         PrintCh('0' + (long)(Num + 1e-7), Stream);
   } else
      PrintCh('0', Stream);
   if (Exponent != 0) {
      PrintCh('e', Stream);
      PrintInt(Exponent, 0, false, false, Stream);
   }
}
#endif

// Intrinsic functions made available to the language.
static void GenericPrintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs, OutputStream Stream) {
   char *FPos;
   Value NextArg = Param[0];
   ValueType FormatType;
   int ArgCount = 1;
   bool LeftJustify = false;
   bool ZeroPad = false;
   int FieldWidth = 0;
   char *Format = Param[0]->Val->Pointer;
   for (FPos = Format; *FPos != '\0'; FPos++) {
      if (*FPos == '%') {
         FPos++;
         FieldWidth = 0;
         if (*FPos == '-') {
         // A leading '-' means left justify.
            LeftJustify = true;
            FPos++;
         }
         if (*FPos == '0') {
         // A leading zero means zero pad a decimal number.
            ZeroPad = true;
            FPos++;
         }
      // Get any field width in the format.
         while (isdigit((int)*FPos))
            FieldWidth = FieldWidth*10 + (*FPos++ - '0');
      // Now check the format type.
         switch (*FPos) {
            case 's':
               FormatType = CharPtrType;
            break;
            case 'd':
            case 'u':
            case 'x':
            case 'b':
            case 'c':
               FormatType = &IntType;
            break;
#ifndef NO_FP
            case 'f':
               FormatType = &FPType;
            break;
#endif
            case '%':
               PrintCh('%', Stream);
               FormatType = NULL;
            break;
            case '\0':
               FPos--;
               FormatType = NULL;
            break;
            default:
               PrintCh(*FPos, Stream);
               FormatType = NULL;
            break;
         }
         if (FormatType != NULL) {
         // We have to format something.
            if (ArgCount >= NumArgs)
               PrintStr("XXX", Stream); // Not enough parameters for format.
            else {
               NextArg = (Value)((char *)NextArg + MEM_ALIGN(sizeof *NextArg + TypeStackSizeValue(NextArg)));
               if (NextArg->Typ != FormatType && !((FormatType == &IntType || *FPos == 'f') && IS_NUMERIC_COERCIBLE(NextArg)) && !(FormatType == CharPtrType && (NextArg->Typ->Base == TypePointer || (NextArg->Typ->Base == TypeArray && NextArg->Typ->FromType->Base == TypeChar))))
                  PrintStr("XXX", Stream); // Bad type for format.
               else {
                  switch (*FPos) {
                     case 's': {
                        char *Str;
                        if (NextArg->Typ->Base == TypePointer)
                           Str = NextArg->Val->Pointer;
                        else
                           Str = &NextArg->Val->ArrayMem[0];
                        if (Str == NULL)
                           PrintStr("NULL", Stream);
                        else
                           PrintStr(Str, Stream);
                     }
                     break;
                     case 'd':
                        PrintInt(ExpressionCoerceInteger(NextArg), FieldWidth, ZeroPad, LeftJustify, Stream);
                     break;
                     case 'u':
                        PrintUnsigned(ExpressionCoerceUnsignedInteger(NextArg), 10, FieldWidth, ZeroPad, LeftJustify, Stream);
                     break;
                     case 'x':
                        PrintUnsigned(ExpressionCoerceUnsignedInteger(NextArg), 16, FieldWidth, ZeroPad, LeftJustify, Stream);
                     break;
                     case 'b':
                        PrintUnsigned(ExpressionCoerceUnsignedInteger(NextArg), 2, FieldWidth, ZeroPad, LeftJustify, Stream);
                     break;
                     case 'c':
                        PrintCh(ExpressionCoerceUnsignedInteger(NextArg), Stream);
                     break;
#ifndef NO_FP
                     case 'f':
                        PrintFP(ExpressionCoerceFP(NextArg), Stream);
                     break;
#endif
                  }
               }
            }
            ArgCount++;
         }
      } else
         PrintCh(*FPos, Stream);
   }
}

// printf(): print to console output.
void LibPrintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct OutputStream ConsoleStream;
   ConsoleStream.Putch = &PlatformPutc;
   GenericPrintf(Parser, ReturnValue, Param, NumArgs, &ConsoleStream);
}

// sprintf(): print to a string.
static void LibSPrintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct OutputStream StrStream;
   StrStream.Putch = &SPutc;
   StrStream.StrI.Parser = Parser;
   StrStream.StrI.WritePos = Param[0]->Val->Pointer;
   GenericPrintf(Parser, ReturnValue, Param + 1, NumArgs - 1, &StrStream);
   PrintCh(0, &StrStream);
   ReturnValue->Val->Pointer = *Param;
}

// Get a line of input.
// Protected from buffer overrun.
static void LibGets(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = PlatformGetLine(Param[0]->Val->Pointer, GETS_BUF_MAX, NULL);
   if (ReturnValue->Val->Pointer != NULL) {
      char *EOLPos = strchr(Param[0]->Val->Pointer, '\n');
      if (EOLPos != NULL)
         *EOLPos = '\0';
   }
}

static void LibGetc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = PlatformGetCharacter();
}

static void LibExit(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   PlatformExit(Param[0]->Val->Integer);
}

#ifdef PICOC_LIBRARY
static void LibSin(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = sin(Param[0]->Val->FP);
}

static void LibCos(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = cos(Param[0]->Val->FP);
}

static void LibTan(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = tan(Param[0]->Val->FP);
}

static void LibAsin(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = asin(Param[0]->Val->FP);
}

static void LibAcos(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = acos(Param[0]->Val->FP);
}

static void LibAtan(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = atan(Param[0]->Val->FP);
}

static void LibSinh(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = sinh(Param[0]->Val->FP);
}

static void LibCosh(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = cosh(Param[0]->Val->FP);
}

static void LibTanh(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = tanh(Param[0]->Val->FP);
}

static void LibExp(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = exp(Param[0]->Val->FP);
}

static void LibFabs(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = fabs(Param[0]->Val->FP);
}

static void LibLog(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = log(Param[0]->Val->FP);
}

static void LibLog10(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = log10(Param[0]->Val->FP);
}

static void LibPow(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = pow(Param[0]->Val->FP, Param[1]->Val->FP);
}

static void LibSqrt(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = sqrt(Param[0]->Val->FP);
}

static void LibRound(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = floor(Param[0]->Val->FP + 0.5); // XXX - fix for soft float.
}

static void LibCeil(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = ceil(Param[0]->Val->FP);
}

static void LibFloor(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = floor(Param[0]->Val->FP);
}
#endif

#ifndef NO_STRING_FUNCTIONS
static void LibMalloc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = malloc(Param[0]->Val->Integer);
}

#   ifndef NO_CALLOC
static void LibCalloc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = calloc(Param[0]->Val->Integer, Param[1]->Val->Integer);
}
#   endif

#   ifndef NO_REALLOC
static void LibRealloc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = realloc(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}
#   endif

static void LibFree(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   free(Param[0]->Val->Pointer);
}

static void LibStrcpy(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   char *To = (char *)Param[0]->Val->Pointer;
   char *From = (char *)Param[1]->Val->Pointer;
   while (*From != '\0')
      *To++ = *From++;
   *To = '\0';
}

static void LibStrncpy(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   char *To = (char *)Param[0]->Val->Pointer;
   char *From = (char *)Param[1]->Val->Pointer;
   int Len = Param[2]->Val->Integer;
   for (; *From != '\0' && Len > 0; Len--)
      *To++ = *From++;
   if (Len > 0)
      *To = '\0';
}

static void LibStrcmp(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   char *Str1 = (char *)Param[0]->Val->Pointer;
   char *Str2 = (char *)Param[1]->Val->Pointer;
   bool StrEnded;
   for (StrEnded = false; !StrEnded; StrEnded = (*Str1 == '\0' || *Str2 == '\0'), Str1++, Str2++) {
      if (*Str1 < *Str2) {
         ReturnValue->Val->Integer = -1;
         return;
      } else if (*Str1 > *Str2) {
         ReturnValue->Val->Integer = 1;
         return;
      }
   }
   ReturnValue->Val->Integer = 0;
}

static void LibStrncmp(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   char *Str1 = (char *)Param[0]->Val->Pointer;
   char *Str2 = (char *)Param[1]->Val->Pointer;
   int Len = Param[2]->Val->Integer;
   bool StrEnded;
   for (StrEnded = false; !StrEnded && Len > 0; StrEnded = (*Str1 == '\0' || *Str2 == '\0'), Str1++, Str2++, Len--) {
      if (*Str1 < *Str2) {
         ReturnValue->Val->Integer = -1;
         return;
      } else if (*Str1 > *Str2) {
         ReturnValue->Val->Integer = 1;
         return;
      }
   }
   ReturnValue->Val->Integer = 0;
}

static void LibStrcat(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   char *To = (char *)Param[0]->Val->Pointer;
   char *From = (char *)Param[1]->Val->Pointer;
   while (*To != '\0')
      To++;
   while (*From != '\0')
      *To++ = *From++;
   *To = '\0';
}

static void LibIndex(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   char *Pos = (char *)Param[0]->Val->Pointer;
   int SearchChar = Param[1]->Val->Integer;
   while (*Pos != '\0' && *Pos != SearchChar)
      Pos++;
   if (*Pos != SearchChar)
      ReturnValue->Val->Pointer = NULL;
   else
      ReturnValue->Val->Pointer = Pos;
}

static void LibRindex(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   char *Pos = (char *)Param[0]->Val->Pointer;
   int SearchChar = Param[1]->Val->Integer;
   ReturnValue->Val->Pointer = NULL;
   for (; *Pos != '\0'; Pos++) {
      if (*Pos == SearchChar)
         ReturnValue->Val->Pointer = Pos;
   }
}

static void LibStrlen(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   char *Pos = (char *)Param[0]->Val->Pointer;
   int Len;
   for (Len = 0; *Pos != '\0'; Pos++)
      Len++;
   ReturnValue->Val->Integer = Len;
}

static void LibMemset(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
// We can use the system memset().
   memset(Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Integer);
}

static void LibMemcpy(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
// We can use the system memcpy().
   memcpy(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

static void LibMemcmp(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   unsigned char *Mem1 = (unsigned char *)Param[0]->Val->Pointer;
   unsigned char *Mem2 = (unsigned char *)Param[1]->Val->Pointer;
   int Len = Param[2]->Val->Integer;
   for (; Len > 0; Mem1++, Mem2++, Len--) {
      if (*Mem1 < *Mem2) {
         ReturnValue->Val->Integer = -1;
         return;
      } else if (*Mem1 > *Mem2) {
         ReturnValue->Val->Integer = 1;
         return;
      }
   }
   ReturnValue->Val->Integer = 0;
}
#endif

// List of all library functions and their prototypes.
struct LibraryFunction CLibrary[] = {
   { LibPrintf, "void printf(char *, ...);" },
   { LibSPrintf, "char *sprintf(char *, char *, ...);" },
   { LibGets, "char *gets(char *);" },
   { LibGetc, "int getchar();" },
   { LibExit, "void exit(int);" },
#ifdef PICOC_LIBRARY
   { LibSin, "float sin(float);" },
   { LibCos, "float cos(float);" },
   { LibTan, "float tan(float);" },
   { LibAsin, "float asin(float);" },
   { LibAcos, "float acos(float);" },
   { LibAtan, "float atan(float);" },
   { LibSinh, "float sinh(float);" },
   { LibCosh, "float cosh(float);" },
   { LibTanh, "float tanh(float);" },
   { LibExp, "float exp(float);" },
   { LibFabs, "float fabs(float);" },
   { LibLog, "float log(float);" },
   { LibLog10, "float log10(float);" },
   { LibPow, "float pow(float, float);" },
   { LibSqrt, "float sqrt(float);" },
   { LibRound, "float round(float);" },
   { LibCeil, "float ceil(float);" },
   { LibFloor, "float floor(float);" },
#endif
   { LibMalloc, "void *malloc(int);" },
#ifndef NO_CALLOC
   { LibCalloc, "void *calloc(int, int);" },
#endif
#ifndef NO_REALLOC
   { LibRealloc, "void *realloc(void *, int);" },
#endif
   { LibFree, "void free(void *);" },
#ifndef NO_STRING_FUNCTIONS
   { LibStrcpy, "void strcpy(char *, char *);" },
   { LibStrncpy, "void strncpy(char *, char *, int);" },
   { LibStrcmp, "int strcmp(char *, char *);" },
   { LibStrncmp, "int strncmp(char *, char *, int);" },
   { LibStrcat, "void strcat(char *, char *);" },
   { LibIndex, "char *index(char *, int);" },
   { LibRindex, "char *rindex(char *, int);" },
   { LibStrlen, "int strlen(char *);" },
   { LibMemset, "void memset(void *, int, int);" },
   { LibMemcpy, "void memcpy(void *, void *, int);" },
   { LibMemcmp, "int memcmp(void *, void *, int);" },
#endif
   { NULL, NULL }
};

#endif // BUILTIN_MINI_STDLIB.
