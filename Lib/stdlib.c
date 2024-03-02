// stdlib.h library for large systems:
// Small embedded systems use Lib.c instead.
#include "../Extern.h"

#ifndef BUILTIN_MINI_STDLIB

static int Stdlib_ZeroValue = 0;

#ifndef NO_FP
void StdlibAtof(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = atof(Param[0]->Val->Pointer);
}
#endif

void StdlibAtoi(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = atoi(Param[0]->Val->Pointer);
}

void StdlibAtol(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = atol(Param[0]->Val->Pointer);
}

#ifndef NO_FP
void StdlibStrtod(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->FP = strtod(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}
#endif

void StdlibStrtol(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = strtol(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

void StdlibStrtoul(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = strtoul(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Integer);
}

void StdlibMalloc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = malloc(Param[0]->Val->Integer);
}

void StdlibCalloc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = calloc(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void StdlibRealloc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = realloc(Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void StdlibFree(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   free(Param[0]->Val->Pointer);
}

void StdlibRand(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = rand();
}

void StdlibSrand(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   srand(Param[0]->Val->Integer);
}

void StdlibAbort(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ProgramFail(Parser, "abort");
}

void StdlibExit(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   PlatformExit(Parser->pc, Param[0]->Val->Integer);
}

void StdlibGetenv(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = getenv(Param[0]->Val->Pointer);
}

void StdlibSystem(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = system(Param[0]->Val->Pointer);
}

#if 0
void StdlibBsearch(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = bsearch(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Integer, Param[3]->Val->Integer, (int (*)())Param[4]->Val->Pointer);
}
#endif

void StdlibAbs(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = abs(Param[0]->Val->Integer);
}

void StdlibLabs(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = labs(Param[0]->Val->Integer);
}

#if 0
void StdlibDiv(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = div(Param[0]->Val->Integer, Param[1]->Val->Integer);
}

void StdlibLdiv(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = ldiv(Param[0]->Val->Integer, Param[1]->Val->Integer);
}
#endif

#if 0
// Handy structure definitions.
const char StdlibDefs[] =
   "typedef struct { "
   "    int quot, rem; "
   "} div_t; "
   ""
   "typedef struct { "
   "    int quot, rem; "
   "} ldiv_t; ";
#endif

// All stdlib.h functions.
struct LibraryFunction StdlibFunctions[] = {
#ifndef NO_FP
   { StdlibAtof, "float atof(char *);" },
   { StdlibStrtod, "float strtod(char *, char **);" },
#endif
   { StdlibAtoi, "int atoi(char *);" },
   { StdlibAtol, "int atol(char *);" },
   { StdlibStrtol, "int strtol(char *, char **, int);" },
   { StdlibStrtoul, "int strtoul(char *, char **, int);" },
   { StdlibMalloc, "void *malloc(int);" },
   { StdlibCalloc, "void *calloc(int, int);" },
   { StdlibRealloc, "void *realloc(void *, int);" },
   { StdlibFree, "void free(void *);" },
   { StdlibRand, "int rand();" },
   { StdlibSrand, "void srand(int);" },
   { StdlibAbort, "void abort();" },
   { StdlibExit, "void exit(int);" },
   { StdlibGetenv, "char *getenv(char *);" },
   { StdlibSystem, "int system(char *);" },
#if 0
   { StdlibBsearch, "void *bsearch(void *, void *, int, int, int (*)());" },
   { StdlibQsort, "void *qsort(void *, int, int, int (*)());" },
#endif
   { StdlibAbs, "int abs(int);" },
   { StdlibLabs, "int labs(int);" },
#if 0
   { StdlibDiv, "div_t div(int);" },
   { StdlibLdiv, "ldiv_t ldiv(int);" },
#endif
   { NULL, NULL }
};

// Creates various system-dependent definitions.
void StdlibSetupFunc(State pc) {
// Define NULL, TRUE and FALSE.
   if (!VariableDefined(pc, TableStrRegister(pc, "NULL")))
      VariableDefinePlatformVar(pc, NULL, "NULL", &pc->IntType, (AnyValue)&Stdlib_ZeroValue, FALSE);
}

#endif // !BUILTIN_MINI_STDLIB.
