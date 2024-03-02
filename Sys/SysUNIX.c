#include "../Main.h"
#include "../Extern.h"
#ifdef USE_READLINE
#   include <readline/readline.h>
#   include <readline/history.h>
#endif

// Mark where to end the program for platforms which require this.
jmp_buf PicocExitBuf;

#ifndef NO_DEBUGGER
#   include <signal.h>

State break_pc = NULL;

static void BreakHandler(int Signal) {
   break_pc->DebugManualBreak = TRUE;
}

void PlatformInit(State pc) {
// Capture the break signal and pass it to the debugger.
   break_pc = pc;
   signal(SIGINT, BreakHandler);
}
#else
void PlatformInit(State pc) {
}
#endif

void PlatformCleanup(State pc) {
}

// Get a line of interactive input.
char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt) {
#ifdef USE_READLINE
   if (Prompt != NULL) {
   // Use GNU readline to read the line.
      char *InLine = readline(Prompt);
      if (InLine == NULL)
         return NULL;
      Buf[MaxLen - 1] = '\0';
      strncpy(Buf, InLine, MaxLen - 2);
      strncat(Buf, "\n", MaxLen - 2);
      if (InLine[0] != '\0')
         add_history(InLine);
      free(InLine);
      return Buf;
   }
#endif
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
// Ignore "#!/path/to/picoc" ... by replacing the "#!" with "//".
   if (SourceStr != NULL && SourceStr[0] == '#' && SourceStr[1] == '!') {
      SourceStr[0] = '/';
      SourceStr[1] = '/';
   }
   PicocParse(pc, FileName, SourceStr, strlen(SourceStr), TRUE, FALSE, TRUE, TRUE);
}

// Exit the program.
void PlatformExit(State pc, int RetVal) {
   pc->PicocExitValue = RetVal;
   longjmp(pc->PicocExitBuf, 1);
}
