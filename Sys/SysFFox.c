#include "../Extern.h"

// Deallocate any storage.
void PlatformCleanup(State pc) {
}

// Get a line of interactive input.
char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt) {
// XXX - unimplemented so far.
   return NULL;
}

// Get a character of interactive input.
int PlatformGetCharacter() {
// XXX - unimplemented so far.
   return 0;
}

// Write a character to the console.
void PlatformPutc(unsigned char OutCh, OutputStreamInfo Stream) {
// XXX - unimplemented so far.
}

// Read a file into memory.
char *PlatformReadFile(State pc, const char *FileName) {
// XXX - unimplemented so far.
   return NULL;
}

// Read and scan a file for definitions.
void PicocPlatformScanFile(State pc, const char *FileName) {
   char *SourceStr = PlatformReadFile(pc, FileName);
   Parse(FileName, SourceStr, strlen(SourceStr), TRUE);
#if 0
   free(SourceStr);
#endif
}

// Mark where to end the program for platforms which require this.
jmp_buf ExitBuf;

// Exit the program.
void PlatformExit(State pc, int RetVal) {
   longjmp(pc->PicocExitBuf, 1);
}
