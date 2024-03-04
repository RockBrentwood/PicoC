// stdio.h library for large systems:
// Small embedded systems use Lib.c instead.
#ifndef BUILTIN_MINI_STDLIB

#include <errno.h>
#include "../Extern.h"

#define FormatMax 80
#define ScanArgMax 10

static int Stdio_ZeroValue = 0;
static int EOFValue = EOF;
static int SEEK_SETValue = SEEK_SET;
static int SEEK_CURValue = SEEK_CUR;
static int SEEK_ENDValue = SEEK_END;
static int BUFSIZValue = BUFSIZ;
static int FILENAME_MAXValue = FILENAME_MAX;
static int _IOFBFValue = _IOFBF;
static int _IOLBFValue = _IOLBF;
static int _IONBFValue = _IONBF;
static int L_tmpnamValue = L_tmpnam;
static int GETS_MAXValue = 255; // Arbitrary maximum size of a gets() file.
static FILE *stdinValue;
static FILE *stdoutValue;
static FILE *stderrValue;

// Our own internal output stream which can output to FILE * or strings.
typedef struct StdOutStream {
   FILE *FilePtr;
   char *StrOutPtr;
   int StrOutLen;
   int CharCount;
} *StdOutStream;

// Our representation of varargs within PicoC.
typedef struct StdVararg {
   Value *Param;
   int NumArgs;
} *StdVararg;

// Initializes the I/O system so error reporting works.
void BasicIOInit(State pc) {
   pc->CStdOut = stdout;
   stdinValue = stdin;
   stdoutValue = stdout;
   stderrValue = stderr;
}

// Output a single character to either a FILE * or a string.
void StdioOutPutc(int OutCh, StdOutStream Stream) {
   if (Stream->FilePtr != NULL) {
   // Output to stdio stream.
      putc(OutCh, Stream->FilePtr);
      Stream->CharCount++;
   } else if (Stream->StrOutLen < 0 || Stream->StrOutLen > 1) {
   // Output to a string.
      *Stream->StrOutPtr = OutCh;
      Stream->StrOutPtr++;
      if (Stream->StrOutLen > 1)
         Stream->StrOutLen--;
      Stream->CharCount++;
   }
}

// Output a string to either a FILE * or a string.
void StdioOutPuts(const char *Str, StdOutStream Stream) {
   if (Stream->FilePtr != NULL) {
   // Output to stdio stream.
      fputs(Str, Stream->FilePtr);
   } else {
   // Output to a string.
      while (*Str != '\0') {
         if (Stream->StrOutLen < 0 || Stream->StrOutLen > 1) {
         // Output to a string.
            *Stream->StrOutPtr = *Str;
            Str++;
            Stream->StrOutPtr++;
            if (Stream->StrOutLen > 1)
               Stream->StrOutLen--;
            Stream->CharCount++;
         }
      }
   }
}

// printf-style format of an int or other word-sized object.
void StdioFprintfWord(StdOutStream Stream, const char *Format, unsigned long Value) {
   if (Stream->FilePtr != NULL)
      Stream->CharCount += fprintf(Stream->FilePtr, Format, Value);
   else if (Stream->StrOutLen >= 0) {
#ifndef WIN32
      int CCount = snprintf(Stream->StrOutPtr, Stream->StrOutLen, Format, Value);
#else
      int CCount = _snprintf(Stream->StrOutPtr, Stream->StrOutLen, Format, Value);
#endif
      Stream->StrOutPtr += CCount;
      Stream->StrOutLen -= CCount;
      Stream->CharCount += CCount;
   } else {
      int CCount = sprintf(Stream->StrOutPtr, Format, Value);
      Stream->CharCount += CCount;
      Stream->StrOutPtr += CCount;
   }
}

// printf-style format of a floating point number.
void StdioFprintfFP(StdOutStream Stream, const char *Format, double Value) {
   if (Stream->FilePtr != NULL)
      Stream->CharCount += fprintf(Stream->FilePtr, Format, Value);
   else if (Stream->StrOutLen >= 0) {
#ifndef WIN32
      int CCount = snprintf(Stream->StrOutPtr, Stream->StrOutLen, Format, Value);
#else
      int CCount = _snprintf(Stream->StrOutPtr, Stream->StrOutLen, Format, Value);
#endif
      Stream->StrOutPtr += CCount;
      Stream->StrOutLen -= CCount;
      Stream->CharCount += CCount;
   } else {
      int CCount = sprintf(Stream->StrOutPtr, Format, Value);
      Stream->CharCount += CCount;
      Stream->StrOutPtr += CCount;
   }
}

// printf-style format of a pointer.
void StdioFprintfPointer(StdOutStream Stream, const char *Format, void *Value) {
   if (Stream->FilePtr != NULL)
      Stream->CharCount += fprintf(Stream->FilePtr, Format, Value);
   else if (Stream->StrOutLen >= 0) {
#ifndef WIN32
      int CCount = snprintf(Stream->StrOutPtr, Stream->StrOutLen, Format, Value);
#else
      int CCount = _snprintf(Stream->StrOutPtr, Stream->StrOutLen, Format, Value);
#endif
      Stream->StrOutPtr += CCount;
      Stream->StrOutLen -= CCount;
      Stream->CharCount += CCount;
   } else {
      int CCount = sprintf(Stream->StrOutPtr, Format, Value);
      Stream->CharCount += CCount;
      Stream->StrOutPtr += CCount;
   }
}

// Internal do-anything v[s][n]printf() formatting system with output to strings or FILE *.
int StdioBasePrintf(ParseState Parser, FILE *Stream, char *StrOut, int StrOutLen, char *Format, StdVararg Args) {
   Value ThisArg = Args->Param[0];
   int ArgCount = 0;
   char *FPos;
   char OneFormatBuf[FormatMax + 1];
   int OneFormatCount;
   ValueType Type;
   struct StdOutStream SOStream;
   State pc = Parser->pc;
   if (Format == NULL)
      Format = "[null format]\n";
   FPos = Format;
   SOStream.FilePtr = Stream;
   SOStream.StrOutPtr = StrOut;
   SOStream.StrOutLen = StrOutLen;
   SOStream.CharCount = 0;
   while (*FPos != '\0') {
      if (*FPos == '%') {
      // Work out what type we're printing.
         FPos++;
         Type = NULL;
         OneFormatBuf[0] = '%';
         OneFormatCount = 1;
         do {
            switch (*FPos) {
            // Integer decimal.
               case 'd': case 'i': Type = &pc->IntType; break;
            // Integer base conversions.
               case 'o': case 'u': case 'x': case 'X': Type = &pc->IntType; break;
#ifndef NO_FP
            // Double, exponent form.
               case 'e': case 'E': Type = &pc->FPType; break;
            // Double, fixed-point.
               case 'f': case 'F': Type = &pc->FPType; break;
            // Double, flexible format.
               case 'g': case 'G': Type = &pc->FPType; break;
#endif
            // Hexadecimal, 0x- format.
               case 'a': case 'A': Type = &pc->IntType; break;
            // Character.
               case 'c': Type = &pc->IntType; break;
            // String.
               case 's': Type = pc->CharPtrType; break;
            // Pointer.
               case 'p': Type = pc->VoidPtrType; break;
            // Number of characters written.
               case 'n': Type = &pc->VoidType; break;
            // strerror(errno).
               case 'm': Type = &pc->VoidType; break;
            // Just a '%' character.
               case '%': Type = &pc->VoidType; break;
            // End of format string.
               case '\0': Type = &pc->VoidType; break;
            }
         // Copy one character of format across to the OneFormatBuf.
            OneFormatBuf[OneFormatCount] = *FPos;
            OneFormatCount++;
         // Do special actions depending on the conversion type.
            if (Type == &pc->VoidType) {
               switch (*FPos) {
                  case 'm': StdioOutPuts(strerror(errno), &SOStream); break;
                  case '%': StdioOutPutc(*FPos, &SOStream); break;
                  case '\0': OneFormatBuf[OneFormatCount] = '\0', StdioOutPutc(*FPos, &SOStream); break;
                  case 'n':
                     ThisArg = (Value)AddAlign(ThisArg, sizeof *ThisArg + TypeStackSizeValue(ThisArg));
                     if (ThisArg->Typ->Base == ArrayT && ThisArg->Typ->FromType->Base == IntT)
                        *(int *)ThisArg->Val->Pointer = SOStream.CharCount;
                  break;
               }
            }
            FPos++;
         } while (Type == NULL && OneFormatCount < FormatMax);
         if (Type != &pc->VoidType) {
            if (ArgCount >= Args->NumArgs)
               StdioOutPuts("XXX", &SOStream);
            else {
            // Null-terminate the buffer.
               OneFormatBuf[OneFormatCount] = '\0';
            // Print this argument.
               ThisArg = (Value)AddAlign(ThisArg, sizeof *ThisArg + TypeStackSizeValue(ThisArg));
               if (Type == &pc->IntType) {
               // Show a signed integer.
                  if (IsNumVal(ThisArg))
                     StdioFprintfWord(&SOStream, OneFormatBuf, ExpressionCoerceUnsignedInteger(ThisArg));
                  else
                     StdioOutPuts("XXX", &SOStream);
               }
#ifndef NO_FP
               else if (Type == &pc->FPType) {
               // Show a floating point number.
                  if (IsNumVal(ThisArg))
                     StdioFprintfFP(&SOStream, OneFormatBuf, ExpressionCoerceFP(ThisArg));
                  else
                     StdioOutPuts("XXX", &SOStream);
               }
#endif
               else if (Type == pc->CharPtrType) {
                  if (ThisArg->Typ->Base == PointerT)
                     StdioFprintfPointer(&SOStream, OneFormatBuf, ThisArg->Val->Pointer);
                  else if (ThisArg->Typ->Base == ArrayT && ThisArg->Typ->FromType->Base == CharT)
                     StdioFprintfPointer(&SOStream, OneFormatBuf, ThisArg->Val->ArrayMem);
                  else
                     StdioOutPuts("XXX", &SOStream);
               } else if (Type == pc->VoidPtrType) {
                  if (ThisArg->Typ->Base == PointerT)
                     StdioFprintfPointer(&SOStream, OneFormatBuf, ThisArg->Val->Pointer);
                  else if (ThisArg->Typ->Base == ArrayT)
                     StdioFprintfPointer(&SOStream, OneFormatBuf, ThisArg->Val->ArrayMem);
                  else
                     StdioOutPuts("XXX", &SOStream);
               }
               ArgCount++;
            }
         }
      } else {
      // Just output a normal character.
         StdioOutPutc(*FPos, &SOStream);
         FPos++;
      }
   }
// Null-terminate.
   if (SOStream.StrOutPtr != NULL && SOStream.StrOutLen > 0)
      *SOStream.StrOutPtr = '\0';
   return SOStream.CharCount;
}

// Internal do-anything v[s][n]scanf() formatting system with input from strings or FILE *.
int StdioBaseScanf(ParseState Parser, FILE *Stream, char *StrIn, char *Format, StdVararg Args) {
   Value ThisArg = Args->Param[0];
   int ArgCount = 0;
   void *ScanfArg[ScanArgMax];
   if (Args->NumArgs > ScanArgMax)
      ProgramFail(Parser, "too many arguments to scanf() - %d max", ScanArgMax);
   for (ArgCount = 0; ArgCount < Args->NumArgs; ArgCount++) {
      ThisArg = (Value)AddAlign(ThisArg, sizeof *ThisArg + TypeStackSizeValue(ThisArg));
      if (ThisArg->Typ->Base == PointerT)
         ScanfArg[ArgCount] = ThisArg->Val->Pointer;
      else if (ThisArg->Typ->Base == ArrayT)
         ScanfArg[ArgCount] = ThisArg->Val->ArrayMem;
      else
         ProgramFail(Parser, "non-pointer argument to scanf() - argument %d after format", ArgCount + 1);
   }
   return Stream != NULL?
      fscanf(Stream, Format, ScanfArg[0], ScanfArg[1], ScanfArg[2], ScanfArg[3], ScanfArg[4], ScanfArg[5], ScanfArg[6], ScanfArg[7], ScanfArg[8], ScanfArg[9]):
      sscanf(StrIn, Format, ScanfArg[0], ScanfArg[1], ScanfArg[2], ScanfArg[3], ScanfArg[4], ScanfArg[5], ScanfArg[6], ScanfArg[7], ScanfArg[8], ScanfArg[9]);
}

// stdio calls.
void StdioFopen(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = fopen(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdioFreopen(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = freopen(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Pointer);
}

void StdioFclose(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fclose(Param[0]->Val->Pointer);
}

void StdioFread(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fread(Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Integer, Param[3]->Val->Pointer);
}

void StdioFwrite(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fwrite(Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Integer, Param[3]->Val->Pointer);
}

void StdioFgetc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fgetc(Param[0]->Val->Pointer);
}

void StdioFgets(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = fgets(Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Pointer);
}

void StdioRemove(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = remove(Param[0]->Val->Pointer);
}

void StdioRename(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = rename(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdioRewind(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   rewind(Param[0]->Val->Pointer);
}

void StdioTmpfile(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = tmpfile();
}

void StdioClearerr(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   clearerr((FILE *) Param[0]->Val->Pointer);
}

void StdioFeof(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = feof((FILE *) Param[0]->Val->Pointer);
}

void StdioFerror(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = ferror((FILE *) Param[0]->Val->Pointer);
}

void StdioFileno(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
#ifndef WIN32
   ReturnValue->Val->Integer = fileno(Param[0]->Val->Pointer);
#else
   ReturnValue->Val->Integer = _fileno(Param[0]->Val->Pointer);
#endif
}

void StdioFflush(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fflush(Param[0]->Val->Pointer);
}

void StdioFgetpos(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fgetpos(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdioFsetpos(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fsetpos(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdioFputc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fputc(Param[0]->Val->Integer, Param[1]->Val->Pointer);
}

void StdioFputs(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fputs(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdioFtell(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = ftell(Param[0]->Val->Pointer);
}

void StdioFseek(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = fseek(Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Integer);
}

void StdioPerror(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   perror(Param[0]->Val->Pointer);
}

void StdioPutc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = putc(Param[0]->Val->Integer, Param[1]->Val->Pointer);
}

void StdioPutchar(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = putchar(Param[0]->Val->Integer);
}

void StdioSetbuf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   setbuf(Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdioSetvbuf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   setvbuf(Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Integer, Param[3]->Val->Integer);
}

void StdioUngetc(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = ungetc(Param[0]->Val->Integer, Param[1]->Val->Pointer);
}

void StdioPuts(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = puts(Param[0]->Val->Pointer);
}

void StdioGets(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Pointer = fgets(Param[0]->Val->Pointer, GETS_MAXValue, stdin);
   if (ReturnValue->Val->Pointer != NULL) {
      char *EOLPos = strchr(Param[0]->Val->Pointer, '\n');
      if (EOLPos != NULL)
         *EOLPos = '\0';
   }
}

void StdioGetchar(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = getchar();
}

void StdioPrintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct StdVararg PrintfArgs;
   PrintfArgs.Param = Param;
   PrintfArgs.NumArgs = NumArgs - 1;
   ReturnValue->Val->Integer = StdioBasePrintf(Parser, stdout, NULL, 0, Param[0]->Val->Pointer, &PrintfArgs);
}

void StdioVprintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = StdioBasePrintf(Parser, stdout, NULL, 0, Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdioFprintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct StdVararg PrintfArgs;
   PrintfArgs.Param = Param + 1;
   PrintfArgs.NumArgs = NumArgs - 2;
   ReturnValue->Val->Integer = StdioBasePrintf(Parser, Param[0]->Val->Pointer, NULL, 0, Param[1]->Val->Pointer, &PrintfArgs);
}

void StdioVfprintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = StdioBasePrintf(Parser, Param[0]->Val->Pointer, NULL, 0, Param[1]->Val->Pointer, Param[2]->Val->Pointer);
}

void StdioSprintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct StdVararg PrintfArgs;
   PrintfArgs.Param = Param + 1;
   PrintfArgs.NumArgs = NumArgs - 2;
   ReturnValue->Val->Integer = StdioBasePrintf(Parser, NULL, Param[0]->Val->Pointer, -1, Param[1]->Val->Pointer, &PrintfArgs);
}

void StdioSnprintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct StdVararg PrintfArgs;
   PrintfArgs.Param = Param + 2;
   PrintfArgs.NumArgs = NumArgs - 3;
   ReturnValue->Val->Integer = StdioBasePrintf(Parser, NULL, Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Pointer, &PrintfArgs);
}

void StdioScanf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct StdVararg ScanfArgs;
   ScanfArgs.Param = Param;
   ScanfArgs.NumArgs = NumArgs - 1;
   ReturnValue->Val->Integer = StdioBaseScanf(Parser, stdin, NULL, Param[0]->Val->Pointer, &ScanfArgs);
}

void StdioFscanf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct StdVararg ScanfArgs;
   ScanfArgs.Param = Param + 1;
   ScanfArgs.NumArgs = NumArgs - 2;
   ReturnValue->Val->Integer = StdioBaseScanf(Parser, Param[0]->Val->Pointer, NULL, Param[1]->Val->Pointer, &ScanfArgs);
}

void StdioSscanf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   struct StdVararg ScanfArgs;
   ScanfArgs.Param = Param + 1;
   ScanfArgs.NumArgs = NumArgs - 2;
   ReturnValue->Val->Integer = StdioBaseScanf(Parser, NULL, Param[0]->Val->Pointer, Param[1]->Val->Pointer, &ScanfArgs);
}

void StdioVsprintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = StdioBasePrintf(Parser, NULL, Param[0]->Val->Pointer, -1, Param[1]->Val->Pointer, Param[2]->Val->Pointer);
}

void StdioVsnprintf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = StdioBasePrintf(Parser, NULL, Param[0]->Val->Pointer, Param[1]->Val->Integer, Param[2]->Val->Pointer, Param[3]->Val->Pointer);
}

void StdioVscanf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = StdioBaseScanf(Parser, stdin, NULL, Param[0]->Val->Pointer, Param[1]->Val->Pointer);
}

void StdioVfscanf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = StdioBaseScanf(Parser, Param[0]->Val->Pointer, NULL, Param[1]->Val->Pointer, Param[2]->Val->Pointer);
}

void StdioVsscanf(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = StdioBaseScanf(Parser, NULL, Param[0]->Val->Pointer, Param[1]->Val->Pointer, Param[2]->Val->Pointer);
}

// Handy structure definitions.
const char StdioDefs[] =
   "typedef struct __va_listStruct va_list; "
   "typedef struct __FILEStruct FILE;";

// All stdio functions.
struct LibraryFunction StdioFunctions[] = {
   { StdioFopen, "FILE *fopen(char *, char *);" },
   { StdioFreopen, "FILE *freopen(char *, char *, FILE *);" },
   { StdioFclose, "int fclose(FILE *);" },
   { StdioFread, "int fread(void *, int, int, FILE *);" },
   { StdioFwrite, "int fwrite(void *, int, int, FILE *);" },
   { StdioFgetc, "int fgetc(FILE *);" },
   { StdioFgetc, "int getc(FILE *);" },
   { StdioFgets, "char *fgets(char *, int, FILE *);" },
   { StdioFputc, "int fputc(int, FILE *);" },
   { StdioFputs, "int fputs(char *, FILE *);" },
   { StdioRemove, "int remove(char *);" },
   { StdioRename, "int rename(char *, char *);" },
   { StdioRewind, "void rewind(FILE *);" },
   { StdioTmpfile, "FILE *tmpfile();" },
   { StdioClearerr, "void clearerr(FILE *);" },
   { StdioFeof, "int feof(FILE *);" },
   { StdioFerror, "int ferror(FILE *);" },
   { StdioFileno, "int fileno(FILE *);" },
   { StdioFflush, "int fflush(FILE *);" },
   { StdioFgetpos, "int fgetpos(FILE *, int *);" },
   { StdioFsetpos, "int fsetpos(FILE *, int *);" },
   { StdioFtell, "int ftell(FILE *);" },
   { StdioFseek, "int fseek(FILE *, int, int);" },
   { StdioPerror, "void perror(char *);" },
   { StdioPutc, "int putc(char *, FILE *);" },
   { StdioPutchar, "int putchar(int);" },
   { StdioPutchar, "int fputchar(int);" },
   { StdioSetbuf, "void setbuf(FILE *, char *);" },
   { StdioSetvbuf, "void setvbuf(FILE *, char *, int, int);" },
   { StdioUngetc, "int ungetc(int, FILE *);" },
   { StdioPuts, "int puts(char *);" },
   { StdioGets, "char *gets(char *);" },
   { StdioGetchar, "int getchar();" },
   { StdioPrintf, "int printf(char *, ...);" },
   { StdioFprintf, "int fprintf(FILE *, char *, ...);" },
   { StdioSprintf, "int sprintf(char *, char *, ...);" },
   { StdioSnprintf, "int snprintf(char *, int, char *, ...);" },
   { StdioScanf, "int scanf(char *, ...);" },
   { StdioFscanf, "int fscanf(FILE *, char *, ...);" },
   { StdioSscanf, "int sscanf(char *, char *, ...);" },
   { StdioVprintf, "int vprintf(char *, va_list);" },
   { StdioVfprintf, "int vfprintf(FILE *, char *, va_list);" },
   { StdioVsprintf, "int vsprintf(char *, char *, va_list);" },
   { StdioVsnprintf, "int vsnprintf(char *, int, char *, va_list);" },
   { StdioVscanf, "int vscanf(char *, va_list);" },
   { StdioVfscanf, "int vfscanf(FILE *, char *, va_list);" },
   { StdioVsscanf, "int vsscanf(char *, char *, va_list);" },
   { NULL, NULL }
};

// Creates various system-dependent definitions.
void StdioSetupFunc(State pc) {
   ValueType StructFileType;
   ValueType FilePtrType;
// Make a "struct __FILEStruct" which is the same size as a native FILE structure.
   StructFileType = TypeCreateOpaqueStruct(pc, NULL, TableStrRegister(pc, "__FILEStruct"), sizeof(FILE));
// Get a FILE * type.
   FilePtrType = TypeGetMatching(pc, NULL, StructFileType, PointerT, 0, pc->StrEmpty, true);
// Make a "struct __va_listStruct" which is the same size as our struct StdVararg.
   TypeCreateOpaqueStruct(pc, NULL, TableStrRegister(pc, "__va_listStruct"), sizeof(FILE));
// Define EOF equal to the system EOF.
   VariableDefinePlatformVar(pc, NULL, "EOF", &pc->IntType, (AnyValue)&EOFValue, false);
   VariableDefinePlatformVar(pc, NULL, "SEEK_SET", &pc->IntType, (AnyValue)&SEEK_SETValue, false);
   VariableDefinePlatformVar(pc, NULL, "SEEK_CUR", &pc->IntType, (AnyValue)&SEEK_CURValue, false);
   VariableDefinePlatformVar(pc, NULL, "SEEK_END", &pc->IntType, (AnyValue)&SEEK_ENDValue, false);
   VariableDefinePlatformVar(pc, NULL, "BUFSIZ", &pc->IntType, (AnyValue)&BUFSIZValue, false);
   VariableDefinePlatformVar(pc, NULL, "FILENAME_MAX", &pc->IntType, (AnyValue)&FILENAME_MAXValue, false);
   VariableDefinePlatformVar(pc, NULL, "_IOFBF", &pc->IntType, (AnyValue)&_IOFBFValue, false);
   VariableDefinePlatformVar(pc, NULL, "_IOLBF", &pc->IntType, (AnyValue)&_IOLBFValue, false);
   VariableDefinePlatformVar(pc, NULL, "_IONBF", &pc->IntType, (AnyValue)&_IONBFValue, false);
   VariableDefinePlatformVar(pc, NULL, "L_tmpnam", &pc->IntType, (AnyValue)&L_tmpnamValue, false);
   VariableDefinePlatformVar(pc, NULL, "GETS_MAX", &pc->IntType, (AnyValue)&GETS_MAXValue, false);
// Define stdin, stdout and stderr.
   VariableDefinePlatformVar(pc, NULL, "stdin", FilePtrType, (AnyValue)&stdinValue, false);
   VariableDefinePlatformVar(pc, NULL, "stdout", FilePtrType, (AnyValue)&stdoutValue, false);
   VariableDefinePlatformVar(pc, NULL, "stderr", FilePtrType, (AnyValue)&stderrValue, false);
// Define NULL, true and false.
   if (!VariableDefined(pc, TableStrRegister(pc, "NULL")))
      VariableDefinePlatformVar(pc, NULL, "NULL", &pc->IntType, (AnyValue)&Stdio_ZeroValue, false);
}

// Portability-related I/O calls.
void PrintCh(char OutCh, FILE *Stream) {
   putc(OutCh, Stream);
}

void PrintSimpleInt(long Num, FILE *Stream) {
   fprintf(Stream, "%ld", Num);
}

void PrintStr(const char *Str, FILE *Stream) {
   fputs(Str, Stream);
}

void PrintFP(double Num, FILE *Stream) {
   fprintf(Stream, "%f", Num);
}

#endif // !BUILTIN_MINI_STDLIB.
