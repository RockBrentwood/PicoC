#include "../Extern.h"
#include "../Main.h"

#if 0
// This was moved into the top-level system structure in Extern.h.
// Mark where to end the program for platforms which require this.
int PicocExitBuf[41];
#endif

// Deallocate any storage.
void PlatformCleanup(State pc) {
}

// Get a line of interactive input.
char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt) {
   printf(Prompt);
   int ix = 0;
   char *cp = NULL;
// If the first character is \n or \r, eat it.
   char ch = getch();
   if (ch == '\n' || ch == '\r') {
   // And get the next character.
      ch = getch();
   }
   while (ix++ < MaxLen) {
      if (ch == 0x1b || ch == 0x03) { // ESC character or ctrl-c (to avoid problem with TeraTerm) - exit.
         printf("Leaving PicoC\n");
         return NULL;
      }
      if (ch == '\n') {
         *cp++ = '\n'; // If newline, send newline character followed by null.
         *cp = 0;
         return Buf;
      }
      *cp++ = ch;
      ix++;
      ch = getch();
   }
   return NULL;
}

// Write a character to the console.
void PlatformPutc(unsigned char OutCh, OutputStreamInfo Stream) {
   if (OutCh == '\n')
      putchar('\r');
   putchar(OutCh);
}

// Read a character.
int PlatformGetCharacter() {
   return getch();
}

// Exit the program.
void PlatformExit(State pc, int RetVal) {
   pc->PicocExitValue = RetVal;
   pc->PicocExitBuf[40] = 1;
   longjmp(pc->PicocExitBuf, 1);
}
