#include "../Main.h"
#include "../Extern.h"

// Mark where to end the program for platforms which require this.
jmp_buf PicocExitBuf;

void PlatformInit(State pc) {
}

void PlatformCleanup(State pc) {
}

// Get a line of interactive input.
char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt) {
   if (Prompt != NULL)
      printf("%s", Prompt);
   fflush(stdout);
   return fgets(Buf, MaxLen, stdin);
}

// Get a character of interactive input.
int PlatformGetCharacter() {
   fflush(stdout);
   return getchar();
}

// Write a character to the console.
void PlatformPutc(unsigned char OutCh, OutputStreamInfo Stream) {
   putchar(OutCh);
}

// Read a file into memory.
char *PlatformReadFile(State pc, const char *FileName) {
   struct stat FileInfo;
   char *ReadText;
   FILE *InFile;
   int BytesRead;
   char *p;
   if (stat(FileName, &FileInfo))
      ProgramFailNoParser(pc, "can't read file %s\n", FileName);
   ReadText = malloc(FileInfo.st_size + 1);
   if (ReadText == NULL)
      ProgramFailNoParser(pc, "out of memory\n");
   InFile = fopen(FileName, "r");
   if (InFile == NULL)
      ProgramFailNoParser(pc, "can't read file %s\n", FileName);
   BytesRead = fread(ReadText, 1, FileInfo.st_size, InFile);
   if (BytesRead == 0)
      ProgramFailNoParser(pc, "can't read file %s\n", FileName);
   ReadText[BytesRead] = '\0';
   fclose(InFile);
   if ((ReadText[0] == '#') && (ReadText[1] == '!')) {
      for (p = ReadText; (*p != '\r') && (*p != '\n'); ++p) {
         *p = ' ';
      }
   }
   return ReadText;
}

// Read and scan a file for definitions.
void PicocPlatformScanFile(State pc, const char *FileName) {
   char *SourceStr = PlatformReadFile(pc, FileName);
   PicocParse(pc, FileName, SourceStr, strlen(SourceStr), true, false, true, true);
}

// Exit the program.
void PlatformExit(State pc, int RetVal) {
   pc->PicocExitValue = RetVal;
   longjmp(pc->PicocExitBuf, 1);
}
