// picoc include system:
// Can emulate system includes from built-in libraries or it can include and parse files if the system has files.
#include "Main.h"
#include "Extern.h"

#ifndef NO_HASH_INCLUDE

// Initialize the built-in include libraries.
void IncludeInit(State pc) {
#ifndef BUILTIN_MINI_STDLIB
   IncludeRegister(pc, "ctype.h", NULL, &StdCtypeFunctions[0], NULL);
   IncludeRegister(pc, "errno.h", &StdErrnoSetupFunc, NULL, NULL);
#   ifndef NO_FP
   IncludeRegister(pc, "math.h", &MathSetupFunc, &MathFunctions[0], NULL);
#   endif
   IncludeRegister(pc, "stdbool.h", &StdboolSetupFunc, NULL, StdboolDefs);
   IncludeRegister(pc, "stdio.h", &StdioSetupFunc, &StdioFunctions[0], StdioDefs);
   IncludeRegister(pc, "stdlib.h", &StdlibSetupFunc, &StdlibFunctions[0], NULL);
   IncludeRegister(pc, "string.h", &StringSetupFunc, &StringFunctions[0], NULL);
   IncludeRegister(pc, "time.h", &StdTimeSetupFunc, &StdTimeFunctions[0], StdTimeDefs);
#   ifndef WIN32
   IncludeRegister(pc, "unistd.h", &UnistdSetupFunc, &UnistdFunctions[0], UnistdDefs);
#   endif
#endif
}

// Clean up space used by the include system.
void IncludeCleanup(State pc) {
   IncludeLibrary ThisInclude = pc->IncludeLibList;
   IncludeLibrary NextInclude;
   while (ThisInclude != NULL) {
      NextInclude = ThisInclude->NextLib;
      HeapFreeMem(pc, ThisInclude);
      ThisInclude = NextInclude;
   }
   pc->IncludeLibList = NULL;
}

// Register a new build-in include file.
void IncludeRegister(State pc, const char *IncludeName, void (*SetupFunction)(State pc), LibraryFunction FuncList, const char *SetupCSource) {
   IncludeLibrary NewLib = HeapAllocMem(pc, sizeof(struct IncludeLibrary));
   NewLib->IncludeName = TableStrRegister(pc, IncludeName);
   NewLib->SetupFunction = SetupFunction;
   NewLib->FuncList = FuncList;
   NewLib->SetupCSource = SetupCSource;
   NewLib->NextLib = pc->IncludeLibList;
   pc->IncludeLibList = NewLib;
}

// Include all of the system headers.
void PicocIncludeAllSystemHeaders(State pc) {
   IncludeLibrary ThisInclude = pc->IncludeLibList;
   for (; ThisInclude != NULL; ThisInclude = ThisInclude->NextLib)
      IncludeFile(pc, ThisInclude->IncludeName);
}

// Include one of a number of predefined libraries, or perhaps an actual file.
void IncludeFile(State pc, char *FileName) {
   IncludeLibrary LInclude;
// Scan for the include file name to see if it's in our list of predefined includes.
   for (LInclude = pc->IncludeLibList; LInclude != NULL; LInclude = LInclude->NextLib) {
      if (strcmp(LInclude->IncludeName, FileName) == 0) {
      // Found it - protect against multiple inclusion.
         if (!VariableDefined(pc, FileName)) {
            VariableDefine(pc, NULL, FileName, NULL, &pc->VoidType, FALSE);
         // Run an extra startup function if there is one.
            if (LInclude->SetupFunction != NULL)
               (*LInclude->SetupFunction)(pc);
         // Parse the setup C source code - may define types etc.
            if (LInclude->SetupCSource != NULL)
               PicocParse(pc, FileName, LInclude->SetupCSource, strlen(LInclude->SetupCSource), TRUE, TRUE, FALSE, FALSE);
         // Set up the library functions.
            if (LInclude->FuncList != NULL)
               LibraryAdd(pc, &pc->GlobalTable, FileName, LInclude->FuncList);
         }
         return;
      }
   }
// Not a predefined file, read a real file.
   PicocPlatformScanFile(pc, FileName);
}

#endif // NO_HASH_INCLUDE.
