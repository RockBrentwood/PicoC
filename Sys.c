// picoc's interface to the underlying platform.
// Most platform-specific code is in Sys/Sys*.c and Sys/Lib*.c.
#include "Main.h"
#include "Extern.h"

// Initialize everything.
void PicocInitialize(State pc, int StackSize) {
   memset(pc, '\0', sizeof *pc);
   PlatformInit(pc);
   BasicIOInit(pc);
   HeapInit(pc, StackSize);
   TableInit(pc);
   VariableInit(pc);
   LexInit(pc);
   TypeInit(pc);
#ifndef NO_HASH_INCLUDE
   IncludeInit(pc);
#endif
   LibraryInit(pc);
#ifdef BUILTIN_MINI_STDLIB
   LibraryAdd(pc, &GlobalTable, "c library", &CLibrary[0]);
   CLibraryInit(pc);
#endif
   PlatformLibraryInit(pc);
   DebugInit(pc);
}

// Free memory.
void PicocCleanup(State pc) {
   DebugCleanup(pc);
#ifndef NO_HASH_INCLUDE
   IncludeCleanup(pc);
#endif
   ParseCleanup(pc);
   LexCleanup(pc);
   VariableCleanup(pc);
   TypeCleanup(pc);
   TableStrFree(pc);
   HeapCleanup(pc);
   PlatformCleanup(pc);
}

// Platform-dependent code for running programs.
#if defined(UNIX_HOST) || defined(WIN32)
#   define CALL_MAIN_NO_ARGS_RETURN_VOID "main();"
#   define CALL_MAIN_WITH_ARGS_RETURN_VOID "main(__argc, __argv);"
#   define CALL_MAIN_NO_ARGS_RETURN_INT "__exit_value = main();"
#   define CALL_MAIN_WITH_ARGS_RETURN_INT "__exit_value = main(__argc, __argv);"

void PicocCallMain(State pc, int argc, char **argv) {
// Check if the program wants arguments.
   Value FuncValue = NULL;
   if (!VariableDefined(pc, TableStrRegister(pc, "main")))
      ProgramFailNoParser(pc, "main() is not defined");
   VariableGet(pc, NULL, TableStrRegister(pc, "main"), &FuncValue);
   if (FuncValue->Typ->Base != FunctionT)
      ProgramFailNoParser(pc, "main is not a function - can't call it");
   if (FuncValue->Val->FuncDef.NumParams != 0) {
   // Define the arguments.
      VariableDefinePlatformVar(pc, NULL, "__argc", &pc->IntType, (AnyValue)&argc, false);
      VariableDefinePlatformVar(pc, NULL, "__argv", pc->CharPtrPtrType, (AnyValue)&argv, false);
   }
   if (FuncValue->Val->FuncDef.ReturnType == &pc->VoidType) {
      if (FuncValue->Val->FuncDef.NumParams == 0)
         PicocParse(pc, "startup", CALL_MAIN_NO_ARGS_RETURN_VOID, strlen(CALL_MAIN_NO_ARGS_RETURN_VOID), true, true, false, true);
      else
         PicocParse(pc, "startup", CALL_MAIN_WITH_ARGS_RETURN_VOID, strlen(CALL_MAIN_WITH_ARGS_RETURN_VOID), true, true, false, true);
   } else {
      VariableDefinePlatformVar(pc, NULL, "__exit_value", &pc->IntType, (AnyValue)&pc->PicocExitValue, true);
      if (FuncValue->Val->FuncDef.NumParams == 0)
         PicocParse(pc, "startup", CALL_MAIN_NO_ARGS_RETURN_INT, strlen(CALL_MAIN_NO_ARGS_RETURN_INT), true, true, false, true);
      else
         PicocParse(pc, "startup", CALL_MAIN_WITH_ARGS_RETURN_INT, strlen(CALL_MAIN_WITH_ARGS_RETURN_INT), true, true, false, true);
   }
}
#endif

void PrintSourceTextErrorLine(OutFile Stream, const char *FileName, const char *SourceText, int Line, int CharacterPos) {
   int LineCount;
   const char *LinePos;
   const char *CPos;
   int CCount;
   if (SourceText != NULL) {
   // Find the source line.
      for (LinePos = SourceText, LineCount = 1; *LinePos != '\0' && LineCount < Line; LinePos++) {
         if (*LinePos == '\n')
            LineCount++;
      }
   // Display the line.
      for (CPos = LinePos; *CPos != '\n' && *CPos != '\0'; CPos++)
         PrintCh(*CPos, Stream);
      PrintCh('\n', Stream);
   // Display the error position.
      for (CPos = LinePos, CCount = 0; *CPos != '\n' && *CPos != '\0' && (CCount < CharacterPos || *CPos == ' '); CPos++, CCount++) {
         if (*CPos == '\t')
            PrintCh('\t', Stream);
         else
            PrintCh(' ', Stream);
      }
   } else {
   // Assume we're in interactive mode - try to make the arrow match up with the input text.
      for (CCount = 0; CCount < CharacterPos + (int)strlen(INTERACTIVE_PROMPT_STATEMENT); CCount++)
         PrintCh(' ', Stream);
   }
   PlatformPrintf(Stream, "^\n%s:%d:%d ", FileName, Line, CharacterPos);
}

static void PlatformVPrintf(OutFile Stream, const char *Format, va_list Args) {
   const char *FPos;
   for (FPos = Format; *FPos != '\0'; FPos++) {
      if (*FPos == '%') {
         FPos++;
         switch (*FPos) {
            case 's': PrintStr(va_arg(Args, char *), Stream); break;
            case 'd': PrintSimpleInt(va_arg(Args, int), Stream); break;
            case 'c': PrintCh(va_arg(Args, int), Stream); break;
            case 't': PrintType(va_arg(Args, ValueType), Stream); break;
#ifndef NO_FP
            case 'f': PrintFP(va_arg(Args, double), Stream); break;
#endif
            case '%': PrintCh('%', Stream); break;
            case '\0': FPos--; break;
         }
      } else
         PrintCh(*FPos, Stream);
   }
}

// Exit with a message.
void ProgramFail(ParseState Parser, const char *Message, ...) {
   va_list Args;
   PrintSourceTextErrorLine(Parser->pc->CStdOut, Parser->FileName, Parser->SourceText, Parser->Line, Parser->CharacterPos);
   va_start(Args, Message);
   PlatformVPrintf(Parser->pc->CStdOut, Message, Args);
   va_end(Args);
   PlatformPrintf(Parser->pc->CStdOut, "\n");
   PlatformExit(Parser->pc, 1);
}

// Exit with a message, when we're not parsing a program.
void ProgramFailNoParser(State pc, const char *Message, ...) {
   va_list Args;
   va_start(Args, Message);
   PlatformVPrintf(pc->CStdOut, Message, Args);
   va_end(Args);
   PlatformPrintf(pc->CStdOut, "\n");
   PlatformExit(pc, 1);
}

// Like ProgramFail() but gives descriptive error messages for assignment.
void AssignFail(ParseState Parser, const char *Format, ValueType Type1, ValueType Type2, int Num1, int Num2, const char *FuncName, int ParamNo) {
   OutFile Stream = Parser->pc->CStdOut;
   PrintSourceTextErrorLine(Parser->pc->CStdOut, Parser->FileName, Parser->SourceText, Parser->Line, Parser->CharacterPos);
   PlatformPrintf(Stream, "can't %s ", (FuncName == NULL)? "assign": "set");
   if (Type1 != NULL)
      PlatformPrintf(Stream, Format, Type1, Type2);
   else
      PlatformPrintf(Stream, Format, Num1, Num2);
   if (FuncName != NULL)
      PlatformPrintf(Stream, " in argument %d of call to %s()", ParamNo, FuncName);
   PlatformPrintf(Stream, "\n");
   PlatformExit(Parser->pc, 1);
}

// Exit lexing with a message.
void LexFail(State pc, LexState Lexer, const char *Message, ...) {
   va_list Args;
   PrintSourceTextErrorLine(pc->CStdOut, Lexer->FileName, Lexer->SourceText, Lexer->Line, Lexer->CharacterPos);
   va_start(Args, Message);
   PlatformVPrintf(pc->CStdOut, Message, Args);
   va_end(Args);
   PlatformPrintf(pc->CStdOut, "\n");
   PlatformExit(pc, 1);
}

// Printf for compiler error reporting.
void PlatformPrintf(OutFile Stream, const char *Format, ...) {
   va_list Args;
   va_start(Args, Format);
   PlatformVPrintf(Stream, Format, Args);
   va_end(Args);
}

// Make a new temporary name.
// Takes a static buffer of char [7] as a parameter.
// Should be initialized to "XX0000" where XX can be any characters.
char *PlatformMakeTempName(State pc, char *TempNameBuffer) {
   int CPos = 5;
   while (CPos > 1) {
      if (TempNameBuffer[CPos] < '9') {
         TempNameBuffer[CPos]++;
         return TableStrRegister(pc, TempNameBuffer);
      } else {
         TempNameBuffer[CPos] = '0';
         CPos--;
      }
   }
   return TableStrRegister(pc, TempNameBuffer);
}
