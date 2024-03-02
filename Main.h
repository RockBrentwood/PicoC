// picoc external interface.
// This should be the only header you need to use if you're using picoc as a library.
// Internal details are in Extern.h.
#ifndef MAIN_H
#define MAIN_H

// picoc version number.
#ifdef VER
#   define PICOC_VERSION "v2.2 beta r" VER // VER is the subversion version number, obtained via the Makefile.
#else
#   define PICOC_VERSION "v2.2"
#endif

// Handy definitions.
#ifndef TRUE
#   define TRUE 1
#   define FALSE 0
#endif

#include "Extern.h"

#if defined(UNIX_HOST) || defined(WIN32)
#   include <setjmp.h>
// This has to be a macro, otherwise errors will occur due to the stack being corrupt.
#   define PicocPlatformSetExitPoint(pc) setjmp((pc)->PicocExitBuf)
#endif
#ifdef SURVEYOR_HOST
// Mark where to end the program for platforms which require this.
extern int PicocExitBuf[];
#   define PicocPlatformSetExitPoint(pc) setjmp((pc)->PicocExitBuf)
#endif

// Syn.c:
void PicocParse(Picoc *pc, const char *FileName, const char *Source, int SourceLen, int RunIt, int CleanupNow, int CleanupSource, int EnableDebugger);
void PicocParseInteractive(Picoc *pc);

// Sys.c:
void PicocCallMain(Picoc *pc, int argc, char **argv);
void PicocInitialize(Picoc *pc, int StackSize);
void PicocCleanup(Picoc *pc);
void PicocPlatformScanFile(Picoc *pc, const char *FileName);

// Inc.c:
void PicocIncludeAllSystemHeaders(Picoc *pc);

#endif // MAIN_H.
