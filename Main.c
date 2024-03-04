// PicoC main program:
// This varies depending on your operating system and how you're using PicoC.

// Include only Main.h here - should be able to use it with only the external interfaces, no internals from Extern.h.
#include "Main.h"

// Platform-dependent code for running programs is in this file.

#if defined UNIX_HOST || defined WIN32
#   include <stdlib.h>
#   include <stdio.h>
#   include <string.h>

int main(int AC, char **AV) {
   char *App = AC < 1? NULL: AV[1]; if (App == NULL || *App == '\0') App = "PicoC";
   if (AC < 2) {
      printf(
         "Format: %s <Source.c>... [- <Arg>...]    : run a program (calls main() to start it)\n"
         "        %s -s <Source.c>... [- <Arg>...] : script mode - runs the program without calling main()\n"
         "        %s -i                            : interactive mode\n",
         App, App, App
      );
      exit(1);
   }
   struct State pc;
   PicocInitialize(&pc, getenv("STACKSIZE")? atoi(getenv("STACKSIZE")): 0x20000);
   int A = 1;
   bool DontRunMain = strcmp(AV[A], "-s") == 0 || strcmp(AV[A], "-m") == 0;
   if (DontRunMain) {
      PicocIncludeAllSystemHeaders(&pc);
      A++;
   }
   if (AC > A && strcmp(AV[A], "-i") == 0) {
      PicocIncludeAllSystemHeaders(&pc);
      PicocParseInteractive(&pc);
   } else {
      if (PicocPlatformSetExitPoint(&pc)) {
         PicocCleanup(&pc);
         return pc.PicocExitValue;
      }
      for (; A < AC && strcmp(AV[A], "-") != 0; A++)
         PicocPlatformScanFile(&pc, AV[A]);
      if (!DontRunMain)
         PicocCallMain(&pc, AC - A, &AV[A]);
   }
   PicocCleanup(&pc);
   return pc.PicocExitValue;
}
#else
#ifdef SURVEYOR_HOST
#   define HEAP_SIZE C_HEAPSIZE
#   include <setjmp.h>
#   include "../srv.h"
#   include "../print.h"
#   include "../string.h"

int PicoC(char *SourceStr) {
   struct State pc;
   PicocInitialize(&pc, HEAP_SIZE);
   if (SourceStr) {
      for (char *pos = SourceStr; *pos != 0; pos++) {
         if (*pos == 0x1a) {
            *pos = 0x20;
         }
      }
   }
   pc.PicocExitBuf[40] = 0;
   PicocPlatformSetExitPoint(&pc);
   if (pc.PicocExitBuf[40]) {
      printf("Leaving PicoC\n\r");
      PicocCleanup(&pc);
      return PicocExitValue;
   }
   if (SourceStr)
      PicocParse(pc, "nofile", SourceStr, strlen(SourceStr), true, true, false, false);
   PicocParseInteractive(&pc);
   PicocCleanup(&pc);
   return pc.PicocExitValue;
}
#endif
#endif
