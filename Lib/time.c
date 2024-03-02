// time.h library for large systems:
// Small embedded systems use Lib.c instead.
#include <time.h>
#include "../Extern.h"

#ifndef BUILTIN_MINI_STDLIB

static int CLOCKS_PER_SECValue = CLOCKS_PER_SEC;

#ifdef CLK_PER_SEC
static int CLK_PER_SECValue = CLK_PER_SEC;
#endif

#ifdef CLK_TCK
static int CLK_TCKValue = CLK_TCK;
#endif

void StdAsctime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = asctime(Param[0]->Val->Pointer);
}

void StdClock(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = clock();
}

void StdCtime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = ctime(Param[0]->Val->Pointer);
}

#ifndef NO_FP
void StdDifftime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = difftime((time_t)Param[0]->Val->Integer, Param[1]->Val->Integer);
}
#endif

void StdGmtime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = gmtime(Param[0]->Val->Pointer);
}

void StdLocaltime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = localtime(Param[0]->Val->Pointer);
}

void StdMktime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = (int)mktime(Param[0]->Val->Pointer);
}

void StdTime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = (int)time(Param[0]->Val->Pointer);
}

void StdStrftime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = strftime(Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Pointer, Param[3]->Val->Pointer);
}

#ifndef WIN32
void StdStrptime(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   extern char *strptime(const char *s, const char *format, struct tm *tm);
   ReturnValue->Val->Pointer = strptime(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Pointer);
}

void StdGmtime_r(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = gmtime_r(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdTimegm(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = timegm(Param[0]->Val->Pointer);
}
#endif

// Handy structure definitions.
const char StdTimeDefs[] =
   "typedef int time_t; "
   "typedef int clock_t;";

// All string.h functions.
struct LibraryFunction StdTimeFunctions[] = {
   { StdAsctime, "char *asctime(struct tm *);" },
   { StdClock, "time_t clock();" },
   { StdCtime, "char *ctime(int *);" },
#ifndef NO_FP
   { StdDifftime, "double difftime(int, int);" },
#endif
   { StdGmtime, "struct tm *gmtime(int *);" },
   { StdLocaltime, "struct tm *localtime(int *);" },
   { StdMktime, "int mktime(struct tm *ptm);" },
   { StdTime, "int time(int *);" },
   { StdStrftime, "int strftime(char *, int, char *, struct tm *);" },
#ifndef WIN32
   { StdStrptime, "char *strptime(char *, char *, struct tm *);" },
   { StdGmtime_r, "struct tm *gmtime_r(int *, struct tm *);" },
   { StdTimegm, "int timegm(struct tm *);" },
#endif
   { NULL, NULL }
};

// Creates various system-dependent definitions.
void StdTimeSetupFunc(State pc) {
// Make a "struct tm" which is the same size as a native tm structure.
   TypeCreateOpaqueStruct(pc, NULL, TableStrRegister(pc, "tm"), sizeof(struct tm));
// Define CLK_PER_SEC etc.
   VariableDefinePlatformVar(pc, NULL, "CLOCKS_PER_SEC", &pc->IntType, (AnyValue)&CLOCKS_PER_SECValue, false);
#ifdef CLK_PER_SEC
   VariableDefinePlatformVar(pc, NULL, "CLK_PER_SEC", &pc->IntType, (AnyValue)&CLK_PER_SECValue, false);
#endif
#ifdef CLK_TCK
   VariableDefinePlatformVar(pc, NULL, "CLK_TCK", &pc->IntType, (AnyValue)&CLK_TCKValue, false);
#endif
}

#endif // !BUILTIN_MINI_STDLIB.
