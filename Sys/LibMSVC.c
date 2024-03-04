#include "../Extern.h"

void MsvcSetupFunc(State pc) {
}

void CTest(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   printf("test(%d)\n", Param[0]->Val->Integer);
   Param[0]->Val->Integer = 1234;
}

void CLineNo(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = Parser->Line;
}

// List of all library functions and their prototypes.
struct LibraryFunction MsvcFunctions[] = {
   { CTest, "void Test(int);" },
   { CLineNo, "int LineNo();" },
   { NULL, NULL }
};

void PlatformLibraryInit(State pc) {
   IncludeRegister(pc, "picoc_msvc.h", &MsvcSetupFunc, MsvcFunctions, NULL);
}
