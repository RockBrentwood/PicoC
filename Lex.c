// PicoC lexer:
// Converts source text into a tokenized form.
#include "Extern.h"

#ifdef NO_CTYPE
#   define isalpha(Ch) (((Ch) >= 'a' && (Ch) <= 'z') || ((Ch) >= 'A' && (Ch) <= 'Z'))
#   define isdigit(Ch) ((Ch) >= '0' && (Ch) <= '9')
#   define isalnum(Ch) (isalpha(Ch) || isdigit(Ch))
#   define isspace(Ch) ((Ch) == ' ' || (Ch) == '\t' || (Ch) == '\r' || (Ch) == '\n')
#endif
#define IsBegId(Ch) (isalpha(Ch) || (Ch) == '_' || (Ch) == '#')
#define IsId(Ch) (isalnum(Ch) || (Ch) == '_')
#define IsHexAlpha(Ch) (((Ch) >= 'a' && (Ch) <= 'f') || ((Ch) >= 'A' && (Ch) <= 'F'))
#define IsBaseDigit(Ch, Base) (((Ch) >= '0' && (Ch) < '0' + ((Base) < 10? (Base): 10)) || ((Base) > 10 && IsHexAlpha(Ch)))
#define GetBaseDigit(Ch) ((Ch) <= '9'? (Ch) - '0': (Ch) <= 'F'? (Ch) - 'A' + 10: (Ch) - 'a' + 10)
static const size_t TokenDataOffset = 2;
#if 0
// Maximum value which can be represented by a "char" data type.
static const char MaxChar = 0xff;
#endif

typedef struct ReservedWord {
   const char *Word;
   Lexical Token;
} *ReservedWord;

static struct ReservedWord ReservedWords[] = {
   { "#define", DefineP },
   { "#else", ElseP },
   { "#endif", EndIfP },
   { "#if", IfP },
   { "#ifdef", IfDefP },
   { "#ifndef", IfNDefP },
   { "#include", IncludeP },
   { "auto", AutoL },
   { "break", BreakL },
   { "case", CaseL },
   { "char", CharL },
   { "continue", ContinueL },
   { "default", DefaultL },
   { "delete", DeleteL },
   { "do", DoL },
#ifndef NO_FP
   { "double", DoubleL },
#endif
   { "else", ElseL },
   { "enum", EnumL },
   { "extern", ExternL },
#ifndef NO_FP
   { "float", FloatL },
#endif
   { "for", ForL },
   { "goto", GotoL },
   { "if", IfL },
   { "int", IntL },
   { "long", LongL },
   { "new", NewL },
   { "register", RegisterL },
   { "return", ReturnL },
   { "short", ShortL },
   { "signed", SignedL },
   { "sizeof", SizeOfL },
   { "static", StaticL },
   { "struct", StructL },
   { "switch", SwitchL },
   { "typedef", TypeDefL },
   { "union", UnionL },
   { "unsigned", UnsignedL },
   { "void", VoidL },
   { "while", WhileL }
};
const size_t ReservedWordN = sizeof ReservedWords/sizeof ReservedWords[0];

// Initialize the lexer.
void LexInit(State pc) {
   TableInitTable(&pc->ReservedWordTable, pc->ReservedWordHashTable, 2*ReservedWordN, true);
   for (int Count = 0; Count < ReservedWordN; Count++) {
      TableSet(pc, &pc->ReservedWordTable, TableStrRegister(pc, ReservedWords[Count].Word), (Value)&ReservedWords[Count], NULL, 0, 0);
   }
   pc->LexValue.Typ = NULL;
   pc->LexValue.Val = &pc->LexAnyValue;
   pc->LexValue.LValueFrom = false;
   pc->LexValue.ValOnHeap = false;
   pc->LexValue.ValOnStack = false;
   pc->LexValue.AnyValOnHeap = false;
   pc->LexValue.IsLValue = false;
}

// Deallocate.
void LexCleanup(State pc) {
   LexInteractiveClear(pc, NULL);
   for (int Count = 0; Count < ReservedWordN; Count++)
      TableDelete(pc, &pc->ReservedWordTable, TableStrRegister(pc, ReservedWords[Count].Word));
}

// Check if a word is a reserved word - used while scanning.
static Lexical LexCheckReservedWord(State pc, const char *Word) {
   Value val;
   return TableGet(&pc->ReservedWordTable, Word, &val, NULL, NULL, NULL)? ((ReservedWord)val)->Token: NoneL;
}

// Lexer state.
typedef struct LexState {
   const char *SourceText, *Pos, *End, *FileName;
   int CharacterPos, Line, EmitExtraNewlines;
   enum { NormalLx, IncludeLx, DefineLx, DeclareLx, NameLx } Mode;
} *LexState;

#define IncLex(L) ((L)->Pos++, (L)->CharacterPos++)
#define AddLex(L, N) ((L)->Pos += (N), (L)->CharacterPos += (N))
#define NextIs2(L, Ch, Ch0, T0, T) ((Ch) == (Ch0)? (IncLex(L), (T0)): (T))
#define NextIs3(L, Ch, Ch1, T1, Ch0, T0, T) ((Ch) == (Ch1)? (IncLex(L), (T1)): NextIs2((L), (Ch), Ch0, T0, T))
#define NextIs4(L, Ch, Ch2, T2, Ch1, T1, Ch0, T0, T) (Ch == (Ch2)? (IncLex(L), (T2)): NextIs3((L), Ch, Ch1, T1, Ch0, T0, T))
#define NextIs3Plus(L, Ch, Ch2, T2, Ch1, T1, Ch0, T0, T) ( \
   (Ch) == (Ch2)? (IncLex(L), (T2)): (Ch) != (Ch1)? (T): (L)->Pos[1] == (Ch0)? (AddLex((L), 2), (T0)): (IncLex(L), (T1)) \
)
#define NextMatches3(L, Ch, Ch1, Ch0, T0, T) ((Ch) == (Ch1) && (L)->Pos[1] == (Ch0)? (AddLex((L), 2), (T0)): (T))

// Get a numeric literal - used while scanning.
static Lexical LexGetNumber(State pc, LexState Lexer, Value Val) {
   long Base = 10;
   if (*Lexer->Pos == '0') {
   // A binary, octal or hex literal.
      IncLex(Lexer);
      if (Lexer->Pos != Lexer->End) {
         if (*Lexer->Pos == 'x' || *Lexer->Pos == 'X') {
            Base = 0x10;
            IncLex(Lexer);
         } else if (*Lexer->Pos == 'b' || *Lexer->Pos == 'B') {
            Base = 2;
            IncLex(Lexer);
         } else if (*Lexer->Pos != '.')
            Base = 010;
      }
   }
// Get the value.
   long Result = 0;
   for (; Lexer->Pos != Lexer->End && IsBaseDigit(*Lexer->Pos, Base); IncLex(Lexer))
      Result = Result*Base + GetBaseDigit(*Lexer->Pos);
// long/unsigned flags.
   bool IsUnsigned = *Lexer->Pos == 'u' || *Lexer->Pos == 'U';
   if (IsUnsigned) {
      IncLex(Lexer);
   }
   bool IsLong = *Lexer->Pos == 'l' || *Lexer->Pos == 'L';
   if (IsLong) {
      IncLex(Lexer);
   }
   Val->Typ = &pc->LongType; // Ignored?
   Val->Val->LongInteger = Result;
   Lexical ResultToken = IntLitL;
   if (Lexer->Pos == Lexer->End)
      return ResultToken;
#ifndef NO_FP
   if (Lexer->Pos == Lexer->End) {
      return ResultToken;
   }
   if (*Lexer->Pos != '.' && *Lexer->Pos != 'e' && *Lexer->Pos != 'E') {
      return ResultToken;
   }
   Val->Typ = &pc->FPType;
   double FPResult = (double)Result;
   if (*Lexer->Pos == '.') {
      IncLex(Lexer);
      for (double FPDiv = 1.0/Base; Lexer->Pos != Lexer->End && IsBaseDigit(*Lexer->Pos, Base); IncLex(Lexer), FPDiv /= (double)Base) {
         FPResult += GetBaseDigit(*Lexer->Pos)*FPDiv;
      }
   }
   if (Lexer->Pos != Lexer->End && (*Lexer->Pos == 'e' || *Lexer->Pos == 'E')) {
      int ExponentSign = +1;
      IncLex(Lexer);
      if (Lexer->Pos != Lexer->End && *Lexer->Pos == '-') {
         ExponentSign = -1;
         IncLex(Lexer);
      }
      Result = 0;
      while (Lexer->Pos != Lexer->End && IsBaseDigit(*Lexer->Pos, Base)) {
         Result = Result*Base + GetBaseDigit(*Lexer->Pos);
         IncLex(Lexer);
      }
      FPResult *= pow((double)Base, (double)Result*ExponentSign);
   }
   Val->Val->FP = FPResult;
   if (*Lexer->Pos == 'f' || *Lexer->Pos == 'F')
      IncLex(Lexer);
   return RatLitL;
#else
   return ResultToken;
#endif
}

// Get a reserved word or identifier - used while scanning.
static Lexical LexGetWord(State pc, LexState Lexer, Value Val) {
   const char *StartPos = Lexer->Pos;
   do {
      IncLex(Lexer);
   } while (Lexer->Pos != Lexer->End && IsId((int)*Lexer->Pos));
   Val->Typ = NULL;
   Val->Val->Identifier = TableStrRegister2(pc, StartPos, Lexer->Pos - StartPos);
   Lexical Token = LexCheckReservedWord(pc, Val->Val->Identifier);
   switch (Token) {
      case IncludeP: Lexer->Mode = IncludeLx; break;
      case DefineP: Lexer->Mode = DefineLx; break;
      default: break;
   }
   if (Token != NoneL)
      return Token;
   if (Lexer->Mode == DeclareLx)
      Lexer->Mode = NameLx;
   return IdL;
}

// Unescape a character from an octal character constant.
static unsigned char LexUnEscapeCharacterConstant(const char **From, const char *End, unsigned char FirstChar, int Base) {
   unsigned char Total = GetBaseDigit(FirstChar);
   for (int CCount = 0; IsBaseDigit(**From, Base) && CCount < 2; CCount++, (*From)++)
      Total = Total*Base + GetBaseDigit(**From);
   return Total;
}

// Unescape a character from a string or character constant.
static unsigned char LexUnEscapeCharacter(const char **From, const char *End) {
   while (*From != End && **From == '\\' && &(*From)[1] != End && (*From)[1] == '\n')
      (*From) += 2; // Skip escaped end of lines with LF line termination.
   while (*From != End && **From == '\\' && &(*From)[1] != End && &(*From)[2] != End && (*From)[1] == '\r' && (*From)[2] == '\n')
      (*From) += 3; // Skip escaped end of lines with CR/LF line termination.
   if (*From == End)
      return '\\';
   if (**From == '\\') {
   // It's escaped.
      (*From)++;
      if (*From == End)
         return '\\';
      unsigned char ThisChar = *(*From)++;
      switch (ThisChar) {
         case 'a': return '\a';
         case 'b': return '\b';
         case 'f': return '\f';
         case 'n': return '\n';
         case 'r': return '\r';
         case 't': return '\t';
         case 'v': return '\v';
         case '0': case '1': case '2': case '3': return LexUnEscapeCharacterConstant(From, End, ThisChar, 010);
         case 'x': return LexUnEscapeCharacterConstant(From, End, '0', 0x10);
         case '\\': case '\'': case '"': default: return ThisChar;
      }
   } else
      return *(*From)++;
}

// Exit lexing with a message.
static void LexError(State pc, LexState Lexer, const char *Message, ...) {
   PrintSourceTextErrorLine(pc->CStdOut, Lexer->FileName, Lexer->SourceText, Lexer->Line, Lexer->CharacterPos);
   va_list Args;
   va_start(Args, Message);
   PlatformVPrintf(pc->CStdOut, Message, Args);
   va_end(Args);
   PlatformPrintf(pc->CStdOut, "\n");
   PlatformExit(pc, 1);
}

// Get a string constant - used while scanning.
static Lexical LexGetStringConstant(State pc, LexState Lexer, Value Val, char EndChar) {
   const char *StartPos = Lexer->Pos;
   bool Escape = false;
   while (Lexer->Pos != Lexer->End && (*Lexer->Pos != EndChar || Escape)) {
   // Find the end.
      if (Escape) {
         if (*Lexer->Pos == '\r' && Lexer->Pos + 1 != Lexer->End)
            Lexer->Pos++;
         if (*Lexer->Pos == '\n' && Lexer->Pos + 1 != Lexer->End) {
            Lexer->Line++;
            Lexer->Pos++;
            Lexer->CharacterPos = 0;
            Lexer->EmitExtraNewlines++;
         }
      }
      Escape = !Escape && *Lexer->Pos == '\\';
      IncLex(Lexer);
   }
   const char *EndPos = Lexer->Pos;
   char *EscBuf = HeapAllocStack(pc, EndPos - StartPos);
   if (EscBuf == NULL)
      LexError(pc, Lexer, "out of memory");
   char *EscBufPos;
   for (EscBufPos = EscBuf, Lexer->Pos = StartPos; Lexer->Pos != EndPos;)
      *EscBufPos++ = LexUnEscapeCharacter(&Lexer->Pos, EndPos);
// Try to find an existing copy of this string literal.
   char *RegString = TableStrRegister2(pc, EscBuf, EscBufPos - EscBuf);
   HeapPopStack(pc, EscBuf, EndPos - StartPos);
   Value ArrayValue = VariableStringLiteralGet(pc, RegString);
   if (ArrayValue == NULL) {
   // Create and store this string literal.
      ArrayValue = VariableAllocValueAndData(pc, NULL, 0, false, NULL, true);
      ArrayValue->Typ = pc->CharArrayType;
      ArrayValue->Val = (AnyValue)RegString;
      VariableStringLiteralDefine(pc, RegString, ArrayValue);
   }
// Create the the pointer for this char *.
   Val->Typ = pc->CharPtrType;
   Val->Val->Pointer = RegString;
   if (*Lexer->Pos == EndChar)
      IncLex(Lexer);
   return StrLitL;
}

// Get a character constant - used while scanning.
static Lexical LexGetCharacterConstant(State pc, LexState Lexer, Value Val) {
   Val->Typ = &pc->CharType;
   Val->Val->Character = LexUnEscapeCharacter(&Lexer->Pos, Lexer->End);
   if (Lexer->Pos != Lexer->End && *Lexer->Pos != '\'')
      LexError(pc, Lexer, "expected \"'\"");
   IncLex(Lexer);
   return CharLitL;
}

// Skip a comment - used while scanning.
void LexSkipComment(LexState Lexer, char NextChar, Lexical *ReturnToken) {
   if (NextChar == '*') {
   // Conventional C comment.
      while (Lexer->Pos != Lexer->End && (*(Lexer->Pos - 1) != '*' || *Lexer->Pos != '/')) {
         if (*Lexer->Pos == '\n')
            Lexer->EmitExtraNewlines++;
         IncLex(Lexer);
      }
      if (Lexer->Pos != Lexer->End)
         IncLex(Lexer);
      Lexer->Mode = NormalLx;
   } else {
   // C++ style comment.
      while (Lexer->Pos != Lexer->End && *Lexer->Pos != '\n')
         IncLex(Lexer);
   }
}

// Get a single token from the source - used while scanning.
static Lexical LexScanGetToken(State pc, LexState Lexer, Value *ValP) {
// Handle cases line multi-line comments or string constants which mess up the line count.
   if (Lexer->EmitExtraNewlines > 0) {
      Lexer->EmitExtraNewlines--;
      return EolL;
   }
// Scan for a token.
   Lexical GotToken = NoneL;
   do {
      *ValP = &pc->LexValue;
      while (Lexer->Pos != Lexer->End && isspace((int)*Lexer->Pos)) {
         if (*Lexer->Pos == '\n') {
            Lexer->Line++;
            Lexer->Pos++;
            Lexer->Mode = NormalLx;
            Lexer->CharacterPos = 0;
            return EolL;
         } else if (Lexer->Mode == DefineLx || Lexer->Mode == DeclareLx)
            Lexer->Mode = DeclareLx;
         else if (Lexer->Mode == NameLx)
            Lexer->Mode = NormalLx;
         IncLex(Lexer);
      }
      if (Lexer->Pos == Lexer->End || *Lexer->Pos == '\0')
         return EofL;
      char ThisChar = *Lexer->Pos;
      if (IsBegId((int)ThisChar))
         return LexGetWord(pc, Lexer, *ValP);
      if (isdigit((int)ThisChar))
         return LexGetNumber(pc, Lexer, *ValP);
      char NextChar = (Lexer->Pos + 1 != Lexer->End)? *(Lexer->Pos + 1): 0;
      IncLex(Lexer);
      switch (ThisChar) {
         case '"': GotToken = LexGetStringConstant(pc, Lexer, *ValP, '"'); break;
         case '\'': GotToken = LexGetCharacterConstant(pc, Lexer, *ValP); break;
         case '(': GotToken = Lexer->Mode == NameLx? LParP: LParL, Lexer->Mode = NormalLx; break;
         case ')': GotToken = RParL; break;
         case '=': GotToken = NextIs2(Lexer, NextChar, '=', RelEqL, EquL); break;
         case '+': GotToken = NextIs3(Lexer, NextChar, '=', AddEquL, '+', IncOpL, AddL); break;
         case '-': GotToken = NextIs4(Lexer, NextChar, '=', SubEquL, '>', ArrowL, '-', DecOpL, SubL); break;
         case '*': GotToken = NextIs2(Lexer, NextChar, '=', MulEquL, StarL); break;
         case '/':
            if (NextChar == '/' || NextChar == '*') {
               IncLex(Lexer);
               LexSkipComment(Lexer, NextChar, &GotToken);
            } else GotToken = NextIs2(Lexer, NextChar, '=', DivEquL, DivL);
         break;
         case '%': GotToken = NextIs2(Lexer, NextChar, '=', ModEquL, ModL); break;
         case '<':
            if (Lexer->Mode == IncludeLx) GotToken = LexGetStringConstant(pc, Lexer, *ValP, '>');
            else {
               GotToken = NextIs3Plus(Lexer, NextChar, '=', RelLeL, '<', ShLL, '=', ShLEquL, RelLtL);
            }
         break;
         case '>': GotToken = NextIs3Plus(Lexer, NextChar, '=', RelGeL, '>', ShRL, '=', ShREquL, RelGtL); break;
         case ';': GotToken = SemiL; break;
         case '&': GotToken = NextIs3(Lexer, NextChar, '=', AndEquL, '&', AndAndL, AndL); break;
         case '|': GotToken = NextIs3(Lexer, NextChar, '=', OrEquL, '|', OrOrL, OrL); break;
         case '{': GotToken = LCurlL; break;
         case '}': GotToken = RCurlL; break;
         case '[': GotToken = LBrL; break;
         case ']': GotToken = RBrL; break;
         case '!': GotToken = NextIs2(Lexer, NextChar, '=', RelNeL, NotL); break;
         case '^': GotToken = NextIs2(Lexer, NextChar, '=', XOrEquL, XOrL); break;
         case '~': GotToken = CplL; break;
         case ',': GotToken = CommaL; break;
         case '.': GotToken = NextMatches3(Lexer, NextChar, '.', '.', DotsL, DotL); break;
         case '?': GotToken = QuestL; break;
         case ':': GotToken = ColonL; break;
         default: LexError(pc, Lexer, "illegal character '%c'", ThisChar); break;
      }
   } while (GotToken == NoneL);
   return GotToken;
}

// What size value goes with each token.
static int LexTokenSize(Lexical Token) {
   switch (Token) {
      case IdL: case StrLitL: return sizeof(char *);
      case IntLitL: return sizeof(long);
      case CharLitL: return sizeof(unsigned char);
      case RatLitL: return sizeof(double);
      default: return 0;
   }
}

// Produce tokens from the lexer and return a heap buffer with the result - used for scanning.
static void *LexTokenize(State pc, LexState Lexer, int *TokenLen) {
   int ReserveSpace = (Lexer->End - Lexer->Pos)*4 + 16;
   void *TokenSpace = HeapAllocStack(pc, ReserveSpace);
   if (TokenSpace == NULL)
      LexError(pc, Lexer, "out of memory");
   char *TokenPos = (char *)TokenSpace;
   Lexical Token;
   int MemUsed = 0;
   int LastCharacterPos = 0;
   do {
   // Store the token at the end of the stack area.
      Value GotValue;
      Token = LexScanGetToken(pc, Lexer, &GotValue);
#ifdef DEBUG_LEXER
      printf("Token: %02x\n", Token);
#endif
      *(unsigned char *)TokenPos = Token;
      TokenPos++;
      MemUsed++;
      *(unsigned char *)TokenPos = (unsigned char)LastCharacterPos;
      TokenPos++;
      MemUsed++;
      int ValueSize = LexTokenSize(Token);
      if (ValueSize > 0) {
      // Store a value as well.
         memcpy((void *)TokenPos, (void *)GotValue->Val, ValueSize);
         TokenPos += ValueSize;
         MemUsed += ValueSize;
      }
      LastCharacterPos = Lexer->CharacterPos;
   } while (Token != EofL);
   void *HeapMem = HeapAllocMem(pc, MemUsed);
   if (HeapMem == NULL)
      LexError(pc, Lexer, "out of memory");
   assert(ReserveSpace >= MemUsed);
   memcpy(HeapMem, TokenSpace, MemUsed);
   HeapPopStack(pc, TokenSpace, ReserveSpace);
#ifdef DEBUG_LEXER
   printf("Tokens: ");
   for (int Count = 0; Count < MemUsed; Count++)
      printf("%02x ", *((unsigned char *)HeapMem + Count));
   printf("\n");
#endif
   if (TokenLen)
      *TokenLen = MemUsed;
   return HeapMem;
}

// Lexically analyse some source text.
void *LexAnalyse(State pc, const char *FileName, const char *Source, int SourceLen, int *TokenLen) {
   struct LexState Lexer;
   Lexer.Pos = Source;
   Lexer.End = Source + SourceLen;
   Lexer.Line = 1;
   Lexer.FileName = FileName;
   Lexer.Mode = NormalLx;
   Lexer.EmitExtraNewlines = 0;
   Lexer.CharacterPos = 1;
   Lexer.SourceText = Source;
   return LexTokenize(pc, &Lexer, TokenLen);
}

// Prepare to parse a pre-tokenized buffer.
void LexInitParser(ParseState Parser, State pc, const char *SourceText, void *TokenSource, char *FileName, bool RunIt, bool EnableDebugger) {
   Parser->pc = pc;
   Parser->Pos = TokenSource;
   Parser->Line = 1;
   Parser->FileName = FileName;
   Parser->Mode = RunIt? RunM: SkipM;
   Parser->SearchLabel = 0;
   Parser->HashIfLevel = 0;
   Parser->HashIfEvaluateToLevel = 0;
   Parser->CharacterPos = 0;
   Parser->SourceText = SourceText;
   Parser->DebugMode = EnableDebugger;
}

// Get the next token, without pre-processing.
static Lexical LexGetRawToken(ParseState Parser, Value *ValP, int IncPos) {
   Lexical Token = NoneL;
   State pc = Parser->pc;
   do {
   // Get the next token.
      if (Parser->Pos == NULL && pc->InteractiveHead != NULL)
         Parser->Pos = pc->InteractiveHead->Tokens;
      if (Parser->FileName != pc->StrEmpty || pc->InteractiveHead != NULL) {
      // Skip leading newlines.
         while ((Token = (Lexical)*(unsigned char *)Parser->Pos) == EolL) {
            Parser->Line++;
            Parser->Pos += TokenDataOffset;
         }
      }
      if (Parser->FileName == pc->StrEmpty && (pc->InteractiveHead == NULL || Token == EofL)) {
      // We're at the end of an interactive input token list.
         if (pc->InteractiveHead == NULL || (unsigned char *)Parser->Pos == &pc->InteractiveTail->Tokens[pc->InteractiveTail->NumBytes - TokenDataOffset]) {
         // Get interactive input.
            char *Prompt = pc->LexUseStatementPrompt? (pc->LexUseStatementPrompt = false, PromptStatement): PromptLine;
            char LineBuffer[LineBufMax];
            if (PlatformGetLine(LineBuffer, LineBufMax, Prompt) == NULL)
               return EofL;
         // Put the new line at the end of the linked list of interactive lines.
            int LineBytes;
            void *LineTokens = LexAnalyse(pc, pc->StrEmpty, LineBuffer, strlen(LineBuffer), &LineBytes);
            TokenLine LineNode = VariableAlloc(pc, Parser, sizeof *LineNode, true);
            LineNode->Tokens = LineTokens;
            LineNode->NumBytes = LineBytes;
            if (pc->InteractiveHead == NULL) {
            // Start a new list.
               pc->InteractiveHead = LineNode;
               Parser->Line = 1;
               Parser->CharacterPos = 0;
            } else
               pc->InteractiveTail->Next = LineNode;
            pc->InteractiveTail = LineNode;
            pc->InteractiveCurrentLine = LineNode;
            Parser->Pos = LineTokens;
         } else {
         // Go to the next token line.
            if (Parser->Pos != &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes - TokenDataOffset]) {
            // Scan for the line.
               for (pc->InteractiveCurrentLine = pc->InteractiveHead; Parser->Pos != &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes - TokenDataOffset]; pc->InteractiveCurrentLine = pc->InteractiveCurrentLine->Next) {
                  assert(pc->InteractiveCurrentLine->Next != NULL);
               }
            }
            assert(pc->InteractiveCurrentLine != NULL);
            pc->InteractiveCurrentLine = pc->InteractiveCurrentLine->Next;
            assert(pc->InteractiveCurrentLine != NULL);
            Parser->Pos = pc->InteractiveCurrentLine->Tokens;
         }
         Token = (Lexical)*(unsigned char *)Parser->Pos;
      }
   } while ((Parser->FileName == pc->StrEmpty && Token == EofL) || Token == EolL);
   Parser->CharacterPos = *((unsigned char *)Parser->Pos + 1);
   int ValueSize = LexTokenSize(Token);
   if (ValueSize > 0) {
   // This token requires a value - unpack it.
      if (ValP != NULL) {
         switch (Token) {
            case StrLitL: pc->LexValue.Typ = pc->CharPtrType; break;
            case IdL: pc->LexValue.Typ = NULL; break;
            case IntLitL: pc->LexValue.Typ = &pc->LongType; break;
            case CharLitL: pc->LexValue.Typ = &pc->CharType; break;
#ifndef NO_FP
            case RatLitL: pc->LexValue.Typ = &pc->FPType; break;
#endif
            default: break;
         }
         memcpy((void *)pc->LexValue.Val, (void *)((char *)Parser->Pos + TokenDataOffset), ValueSize);
         pc->LexValue.ValOnHeap = false;
         pc->LexValue.ValOnStack = false;
         pc->LexValue.IsLValue = false;
         pc->LexValue.LValueFrom = NULL;
         *ValP = &pc->LexValue;
      }
      if (IncPos)
         Parser->Pos += ValueSize + TokenDataOffset;
   } else {
      if (IncPos && Token != EofL)
         Parser->Pos += TokenDataOffset;
   }
#ifdef DEBUG_LEXER
   printf("Got token=%02x inc=%d pos=%d\n", Token, IncPos, Parser->CharacterPos);
#endif
   assert(Token >= NoneL && Token <= EndFnL);
   return Token;
}

// Correct the token position depending if we already incremented the position.
static void LexHashIncPos(ParseState Parser, int IncPos) {
   if (!IncPos)
      LexGetRawToken(Parser, NULL, true);
}

// Handle a #ifdef directive.
static void LexHashIfdef(ParseState Parser, bool IfNot) {
// Get symbol to check.
   Value IdentValue;
   Lexical Token = LexGetRawToken(Parser, &IdentValue, true);
   if (Token != IdL)
      ProgramFail(Parser, "identifier expected");
// Is the identifier defined?
   Value SavedValue;
   bool IsDefined = TableGet(&Parser->pc->GlobalTable, IdentValue->Val->Identifier, &SavedValue, NULL, NULL, NULL);
   if (Parser->HashIfEvaluateToLevel == Parser->HashIfLevel && IsDefined != IfNot) {
   // #if is active, evaluate to this new level.
      Parser->HashIfEvaluateToLevel++;
   }
   Parser->HashIfLevel++;
}

// Handle a #if directive.
static void LexHashIf(ParseState Parser) {
// Get symbol to check.
   Value IdentValue;
   Lexical Token = LexGetRawToken(Parser, &IdentValue, true);
   if (Token == IdL) {
   // Look up a value from a macro definition.
      Value SavedValue = NULL;
      if (!TableGet(&Parser->pc->GlobalTable, IdentValue->Val->Identifier, &SavedValue, NULL, NULL, NULL))
         ProgramFail(Parser, "'%s' is undefined", IdentValue->Val->Identifier);
      if (SavedValue->Typ->Base != MacroT)
         ProgramFail(Parser, "value expected");
      struct ParseState MacroParser;
      ParserCopy(&MacroParser, &SavedValue->Val->MacroDef.Body);
      Token = LexGetRawToken(&MacroParser, &IdentValue, true);
   }
   if (Token != CharLitL && Token != IntLitL)
      ProgramFail(Parser, "value expected");
// Is the identifier defined?
   if (Parser->HashIfEvaluateToLevel == Parser->HashIfLevel && IdentValue->Val->Character) {
   // #if is active, evaluate to this new level.
      Parser->HashIfEvaluateToLevel++;
   }
   Parser->HashIfLevel++;
}

// Handle a #else directive.
static void LexHashElse(ParseState Parser) {
   if (Parser->HashIfEvaluateToLevel == Parser->HashIfLevel - 1)
      Parser->HashIfEvaluateToLevel++; // #if was not active, make this next section active.
   else if (Parser->HashIfEvaluateToLevel == Parser->HashIfLevel) {
   // #if was active, now go inactive.
      if (Parser->HashIfLevel == 0)
         ProgramFail(Parser, "#else without #if");
      Parser->HashIfEvaluateToLevel--;
   }
}

// Handle a #endif directive.
static void LexHashEndif(ParseState Parser) {
   if (Parser->HashIfLevel == 0)
      ProgramFail(Parser, "#endif without #if");
   Parser->HashIfLevel--;
   if (Parser->HashIfEvaluateToLevel > Parser->HashIfLevel)
      Parser->HashIfEvaluateToLevel = Parser->HashIfLevel;
}

#if 0 // Useful for debug.
void LexPrintToken(Lexical Token) {
   char *TokenNames[] = {
      "None", ",",
      "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=",
      "?", ":", "||", "&&",
      "|", "^", "&",
      "==", "!=", "<", ">", "<=", ">=",
      "<<", ">>", "+", "-", "*", "/", "%",
      "++", "--", "!", "~", "sizeof", "Cast",
      "[", "]", ".", "->", "(", ")",
      "Name", "Integer", "Rational", "String", "Character",
      ";", "...", "{", "}",
      "int", "char", "float", "double", "void", "enum", "long", "signed", "short",
      "static", "auto", "register", "extern", "struct", "union", "unsigned", "typedef",
      "continue", "do", "else", "for", "goto", "if", "while", "break", "switch", "case", "default", "return",
      "#define", "#include", "#if", "#ifdef", "#ifndef", "#else", "#endif", "OpenMacro",
      "New", "Delete",
      "EndOfFile", "EndOfLine", "EndOfFunction"
   };
   printf("{%s}", TokenNames[Token]);
}
#endif

// Get the next token given a parser state, pre-processing as we go.
Lexical LexGetToken(ParseState Parser, Value *ValP, int IncPos) {
// Implements the pre-processor #if commands.
   Lexical Token;
   for (bool TryNextToken = true; TryNextToken; ) {
      int WasPreProcToken = true;
      Token = LexGetRawToken(Parser, ValP, IncPos);
      switch (Token) {
         case IfDefP: LexHashIncPos(Parser, IncPos), LexHashIfdef(Parser, false); break;
         case IfNDefP: LexHashIncPos(Parser, IncPos), LexHashIfdef(Parser, true); break;
         case IfP: LexHashIncPos(Parser, IncPos), LexHashIf(Parser); break;
         case ElseP: LexHashIncPos(Parser, IncPos), LexHashElse(Parser); break;
         case EndIfP: LexHashIncPos(Parser, IncPos), LexHashEndif(Parser); break;
         default: WasPreProcToken = false; break;
      }
   // If we're going to reject this token, increment the token pointer to the next one.
      TryNextToken = (Parser->HashIfEvaluateToLevel < Parser->HashIfLevel && Token != EofL) || WasPreProcToken;
      if (!IncPos && TryNextToken)
         LexGetRawToken(Parser, NULL, true);
   }
   return Token;
}

// Take a quick peek at the next token, skipping any pre-processing.
Lexical LexRawPeekToken(ParseState Parser) {
   return (Lexical)*(unsigned char *)Parser->Pos;
}

// Find the end of the line.
void LexToEndOfLine(ParseState Parser) {
   while (true) {
      Lexical Token = (Lexical)*(unsigned char *)Parser->Pos;
      if (Token == EolL || Token == EofL)
         return;
      else
         LexGetRawToken(Parser, NULL, true);
   }
}

// Copy the tokens from StartParser to EndParser into new memory, removing TokenEOFs and terminate with a EndFnL.
void *LexCopyTokens(ParseState StartParser, ParseState EndParser) {
   int MemSize = 0;
   unsigned char *Pos = (unsigned char *)StartParser->Pos;
   unsigned char *NewTokens;
   State pc = StartParser->pc;
   if (pc->InteractiveHead == NULL) {
   // Non-interactive mode - copy the tokens.
      MemSize = EndParser->Pos - StartParser->Pos;
      NewTokens = VariableAlloc(pc, StartParser, MemSize + TokenDataOffset, true);
      memcpy(NewTokens, (void *)StartParser->Pos, MemSize);
   } else {
   // We're in interactive mode - add up line by line.
      for (pc->InteractiveCurrentLine = pc->InteractiveHead; pc->InteractiveCurrentLine != NULL && (Pos < pc->InteractiveCurrentLine->Tokens || Pos >= &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes]); pc->InteractiveCurrentLine = pc->InteractiveCurrentLine->Next) {
      } // Find the line we just counted.
      if (EndParser->Pos >= StartParser->Pos && EndParser->Pos < &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes]) {
      // All on a single line.
         MemSize = EndParser->Pos - StartParser->Pos;
         NewTokens = VariableAlloc(pc, StartParser, MemSize + TokenDataOffset, true);
         memcpy(NewTokens, (void *)StartParser->Pos, MemSize);
      } else {
      // It's spread across multiple lines.
         MemSize = &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes - TokenDataOffset] - Pos;
         TokenLine ILine;
         for (ILine = pc->InteractiveCurrentLine->Next; ILine != NULL && (EndParser->Pos < ILine->Tokens || EndParser->Pos >= &ILine->Tokens[ILine->NumBytes]); ILine = ILine->Next)
            MemSize += ILine->NumBytes - TokenDataOffset;
         assert(ILine != NULL);
         MemSize += EndParser->Pos - ILine->Tokens;
         NewTokens = VariableAlloc(pc, StartParser, MemSize + TokenDataOffset, true);
         int CopySize = &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes - TokenDataOffset] - Pos;
         memcpy(NewTokens, Pos, CopySize);
         unsigned char *NewTokenPos = NewTokens + CopySize;
         for (ILine = pc->InteractiveCurrentLine->Next; ILine != NULL && (EndParser->Pos < ILine->Tokens || EndParser->Pos >= &ILine->Tokens[ILine->NumBytes]); ILine = ILine->Next) {
            memcpy(NewTokenPos, ILine->Tokens, ILine->NumBytes - TokenDataOffset);
            NewTokenPos += ILine->NumBytes - TokenDataOffset;
         }
         assert(ILine != NULL);
         memcpy(NewTokenPos, ILine->Tokens, EndParser->Pos - ILine->Tokens);
      }
   }
   NewTokens[MemSize] = (unsigned char)EndFnL;
   return NewTokens;
}

// Indicate that we've completed up to this point in the interactive input and free expired tokens.
void LexInteractiveClear(State pc, ParseState Parser) {
   while (pc->InteractiveHead != NULL) {
      TokenLine NextLine = pc->InteractiveHead->Next;
      HeapFreeMem(pc, pc->InteractiveHead->Tokens);
      HeapFreeMem(pc, pc->InteractiveHead);
      pc->InteractiveHead = NextLine;
   }
   if (Parser != NULL)
      Parser->Pos = NULL;
   pc->InteractiveTail = NULL;
}

// Indicate that we've completed up to this point in the interactive input and free expired tokens.
void LexInteractiveCompleted(State pc, ParseState Parser) {
   while (pc->InteractiveHead != NULL && !(Parser->Pos >= pc->InteractiveHead->Tokens && Parser->Pos < &pc->InteractiveHead->Tokens[pc->InteractiveHead->NumBytes])) {
   // This token line is no longer needed - free it.
      TokenLine NextLine = pc->InteractiveHead->Next;
      HeapFreeMem(pc, pc->InteractiveHead->Tokens);
      HeapFreeMem(pc, pc->InteractiveHead);
      pc->InteractiveHead = NextLine;
      if (pc->InteractiveHead == NULL) {
      // We've emptied the list.
         Parser->Pos = NULL;
         pc->InteractiveTail = NULL;
      }
   }
}

// The next time we prompt, make it the full statement prompt.
void LexInteractiveStatementPrompt(State pc) {
   pc->LexUseStatementPrompt = true;
}
