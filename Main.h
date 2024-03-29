// PicoC external interface.
// This should be the only header you need to use if you're using PicoC as a library.
// Internal details are in Extern.h.
#ifndef MAIN_H
#define MAIN_H

// PicoC version number.
#ifdef VER
#   define PICOC_VERSION "v2.2 beta r" VER // VER is the subversion version number, obtained via the Makefile.
#else
#   define PICOC_VERSION "v2.2"
#endif

// Handy definitions.
#if 0
// Already defined in Extern.h.
typedef enum { false, true } bool;
#endif
#include "Extern.h"

#if defined UNIX_HOST || defined WIN32
#   include <setjmp.h>
// This has to be a macro, otherwise errors will occur due to the stack being corrupt.
#   define PicocPlatformSetExitPoint(Q) setjmp((Q)->PicocExitBuf)
#endif
#ifdef SURVEYOR_HOST
// Mark where to end the program for platforms which require this.
extern int PicocExitBuf[];
#   define PicocPlatformSetExitPoint(Q) setjmp((Q)->PicocExitBuf)
#endif

// Syn.c:
void PicocParse(State pc, const char *FileName, const char *Source, int SourceLen, bool RunIt, bool CleanupNow, bool CleanupSource, bool EnableDebugger);
void PicocParseInteractive(State pc);

// Sys.c:
void PicocInitialize(State pc, int StackSize);
void PicocCleanup(State pc);
void PicocCallMain(State pc, int AC, char **AV);
// Defined in the following places:
// PicocPlatformSetExitPoint	Main.h as a macro.
// PicocPlatformScanFile	Sys/Sys{UNIX,MSVC,FFox}.c
#if 0
int PicocPlatformSetExitPoint(State pc);
#endif
void PicocPlatformScanFile(State pc, const char *FileName);

// Inc.c:
void PicocIncludeAllSystemHeaders(State pc);

#endif // MAIN_H.
