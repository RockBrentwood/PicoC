#include "../Extern.h"

// Deallocate any storage.
void PlatformCleanup() {
}

// Get a line of interactive input.
char *PlatformGetLine(char *Buf, int MaxLen) {
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
char *PlatformReadFile(const char *FileName) {
// XXX - unimplemented so far.
   return NULL;
}

// Read and scan a file for definitions.
void PlatformScanFile(const char *FileName) {
   char *SourceStr = PlatformReadFile(FileName);
   Parse(FileName, SourceStr, strlen(SourceStr), TRUE);
#if 0
   free(SourceStr);
#endif
}

// Mark where to end the program for platforms which require this.
jmp_buf ExitBuf;

// Exit the program.
void PlatformExit() {
   longjmp(ExitBuf, 1);
}
