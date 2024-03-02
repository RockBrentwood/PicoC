#include "../Extern.h"

void UnixSetupFunc() {
}

void Ctest(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   printf("test(%d)\n", Param[0]->Val->Integer);
   Param[0]->Val->Integer = 1234;
}

void Clineno(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = Parser->Line;
}

// List of all library functions and their prototypes.
struct LibraryFunction UnixFunctions[] = {
   { Ctest, "void test(int);" },
   { Clineno, "int lineno();" },
   { NULL, NULL }
};

void PlatformLibraryInit(State pc) {
   IncludeRegister(pc, "picoc_unix.h", &UnixSetupFunc, &UnixFunctions[0], NULL);
}
