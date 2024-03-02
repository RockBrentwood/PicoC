#include "../Extern.h"
#include "../Main.h"

// Mark where to end the program for platforms which require this.
int PicocExitBuf[41];

// Deallocate any storage.
void PlatformCleanup(State pc) {
}

// Get a line of interactive input.
char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt) {
   int ix;
   char ch, *cp;
   printf(Prompt);
   ix = 0;
   cp = 0;
// If the first character is \n or \r, eat it.
   ch = getch();
   if (ch == '\n' || ch == '\r') {
   // And get the next character.
      ch = getch();
   }
   while (ix++ < MaxLen) {
      if (ch == 0x1B || ch == 0x03) { // ESC character or ctrl-c (to avoid problem with TeraTerm) - exit.
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
