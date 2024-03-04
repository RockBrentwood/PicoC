// picoc lexer:
// Converts source text into a tokenized form.
#include "Extern.h"

#ifdef NO_CTYPE
#   define isalpha(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#   define isdigit(c) ((c) >= '0' && (c) <= '9')
#   define isalnum(c) (isalpha(c) || isdigit(c))
#   define isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
#endif
#define isCidstart(c) (isalpha(c) || (c) == '_' || (c) == '#')
#define isCident(c) (isalnum(c) || (c) == '_')
#define IS_HEX_ALPHA_DIGIT(c) (((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define IS_BASE_DIGIT(c, b) (((c) >= '0' && (c) < '0' + (((b) < 10)? (b): 10)) || (((b) > 10)? IS_HEX_ALPHA_DIGIT(c): false))
#define GET_BASE_DIGIT(c) (((c) <= '9')? ((c) - '0'): (((c) <= 'F')? ((c) - 'A' + 10): ((c) - 'a' + 10)))
#define NEXTIS(c, x, y) { if (NextChar == (c)) { LEXER_INC(Lexer); GotToken = (x); } else GotToken = (y); }
#define NEXTIS3(c, x, d, y, z) { if (NextChar == (c)) { LEXER_INC(Lexer); GotToken = (x); } else NEXTIS(d, y, z) }
#define NEXTIS4(c, x, d, y, e, z, a) { if (NextChar == (c)) { LEXER_INC(Lexer); GotToken = (x); } else NEXTIS3(d, y, e, z, a) }
#define NEXTIS3PLUS(c, x, d, y, e, z, a) { if (NextChar == (c)) { LEXER_INC(Lexer); GotToken = (x); } else if (NextChar == (d)) { if (Lexer->Pos[1] == (e)) { LEXER_INCN(Lexer, 2); GotToken = (z); } else { LEXER_INC(Lexer); GotToken = (y); } } else GotToken = (a); }
#define NEXTISEXACTLY3(c, d, y, z) { if (NextChar == (c) && Lexer->Pos[1] == (d)) { LEXER_INCN(Lexer, 2); GotToken = (y); } else GotToken = (z); }
#define LEXER_INC(l) ((l)->Pos++, (l)->CharacterPos++)
#define LEXER_INCN(l, n) ((l)->Pos += (n), (l)->CharacterPos += (n))
#define TOKEN_DATA_OFFSET 2
#define MAX_CHAR_VALUE 255 // Maximum value which can be represented by a "char" data type.

typedef struct ReservedWord {
   const char *Word;
   LexToken Token;
} *ReservedWord;

static struct ReservedWord ReservedWords[] = {
   { "#define", TokenHashDefine },
   { "#else", TokenHashElse },
   { "#endif", TokenHashEndif },
   { "#if", TokenHashIf },
   { "#ifdef", TokenHashIfdef },
   { "#ifndef", TokenHashIfndef },
   { "#include", TokenHashInclude },
   { "auto", TokenAutoType },
   { "break", TokenBreak },
   { "case", TokenCase },
   { "char", TokenCharType },
   { "continue", TokenContinue },
   { "default", TokenDefault },
   { "delete", TokenDelete },
   { "do", TokenDo },
#ifndef NO_FP
   { "double", TokenDoubleType },
#endif
   { "else", TokenElse },
   { "enum", TokenEnumType },
   { "extern", TokenExternType },
#ifndef NO_FP
   { "float", TokenFloatType },
#endif
   { "for", TokenFor },
   { "goto", TokenGoto },
   { "if", TokenIf },
   { "int", TokenIntType },
   { "long", TokenLongType },
   { "new", TokenNew },
   { "register", TokenRegisterType },
   { "return", TokenReturn },
   { "short", TokenShortType },
   { "signed", TokenSignedType },
   { "sizeof", TokenSizeof },
   { "static", TokenStaticType },
   { "struct", TokenStructType },
   { "switch", TokenSwitch },
   { "typedef", TokenTypedef },
   { "union", TokenUnionType },
   { "unsigned", TokenUnsignedType },
   { "void", TokenVoidType },
   { "while", TokenWhile }
};
const size_t ReservedWordN = sizeof ReservedWords/sizeof ReservedWords[0];

// Initialize the lexer.
void LexInit(State pc) {
   int Count;
   TableInitTable(&pc->ReservedWordTable, &pc->ReservedWordHashTable[0], 2*ReservedWordN, true);
   for (Count = 0; Count < ReservedWordN; Count++) {
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
   int Count;
   LexInteractiveClear(pc, NULL);
   for (Count = 0; Count < ReservedWordN; Count++)
      TableDelete(pc, &pc->ReservedWordTable, TableStrRegister(pc, ReservedWords[Count].Word));
}

// Check if a word is a reserved word - used while scanning.
static LexToken LexCheckReservedWord(State pc, const char *Word) {
   Value val;
   if (TableGet(&pc->ReservedWordTable, Word, &val, NULL, NULL, NULL))
      return ((ReservedWord)val)->Token;
   else
      return TokenNone;
}

// Get a numeric literal - used while scanning.
static LexToken LexGetNumber(State pc, LexState Lexer, Value Val) {
   long Result = 0;
   long Base = 10;
   LexToken ResultToken;
#ifndef NO_FP
   double FPResult;
   double FPDiv;
#endif
// long/unsigned flags.
#if 0 // Unused for now.
   bool IsLong = false;
   bool IsUnsigned = false;
#endif
   if (*Lexer->Pos == '0') {
   // A binary, octal or hex literal.
      LEXER_INC(Lexer);
      if (Lexer->Pos != Lexer->End) {
         if (*Lexer->Pos == 'x' || *Lexer->Pos == 'X') {
            Base = 16;
            LEXER_INC(Lexer);
         } else if (*Lexer->Pos == 'b' || *Lexer->Pos == 'B') {
            Base = 2;
            LEXER_INC(Lexer);
         } else if (*Lexer->Pos != '.')
            Base = 8;
      }
   }
// Get the value.
   for (; Lexer->Pos != Lexer->End && IS_BASE_DIGIT(*Lexer->Pos, Base); LEXER_INC(Lexer))
      Result = Result*Base + GET_BASE_DIGIT(*Lexer->Pos);
   if (*Lexer->Pos == 'u' || *Lexer->Pos == 'U') {
      LEXER_INC(Lexer);
#if 0
      IsUnsigned = true;
#endif
   }
   if (*Lexer->Pos == 'l' || *Lexer->Pos == 'L') {
      LEXER_INC(Lexer);
#if 0
      IsLong = true;
#endif
   }
   Val->Typ = &pc->LongType; // Ignored?
   Val->Val->LongInteger = Result;
   ResultToken = TokenIntegerConstant;
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
   FPResult = (double)Result;
   if (*Lexer->Pos == '.') {
      LEXER_INC(Lexer);
      for (FPDiv = 1.0/Base; Lexer->Pos != Lexer->End && IS_BASE_DIGIT(*Lexer->Pos, Base); LEXER_INC(Lexer), FPDiv /= (double)Base) {
         FPResult += GET_BASE_DIGIT(*Lexer->Pos)*FPDiv;
      }
   }
   if (Lexer->Pos != Lexer->End && (*Lexer->Pos == 'e' || *Lexer->Pos == 'E')) {
      int ExponentSign = 1;
      LEXER_INC(Lexer);
      if (Lexer->Pos != Lexer->End && *Lexer->Pos == '-') {
         ExponentSign = -1;
         LEXER_INC(Lexer);
      }
      Result = 0;
      while (Lexer->Pos != Lexer->End && IS_BASE_DIGIT(*Lexer->Pos, Base)) {
         Result = Result*Base + GET_BASE_DIGIT(*Lexer->Pos);
         LEXER_INC(Lexer);
      }
      FPResult *= pow((double)Base, (double)Result*ExponentSign);
   }
   Val->Val->FP = FPResult;
   if (*Lexer->Pos == 'f' || *Lexer->Pos == 'F')
      LEXER_INC(Lexer);
   return TokenFPConstant;
#else
   return ResultToken;
#endif
}

// Get a reserved word or identifier - used while scanning.
static LexToken LexGetWord(State pc, LexState Lexer, Value Val) {
   const char *StartPos = Lexer->Pos;
   LexToken Token;
   do {
      LEXER_INC(Lexer);
   } while (Lexer->Pos != Lexer->End && isCident((int)*Lexer->Pos));
   Val->Typ = NULL;
   Val->Val->Identifier = TableStrRegister2(pc, StartPos, Lexer->Pos - StartPos);
   Token = LexCheckReservedWord(pc, Val->Val->Identifier);
   switch (Token) {
      case TokenHashInclude:
         Lexer->Mode = LexModeHashInclude;
      break;
      case TokenHashDefine:
         Lexer->Mode = LexModeHashDefine;
      break;
      default:
      break;
   }
   if (Token != TokenNone)
      return Token;
   if (Lexer->Mode == LexModeHashDefineSpace)
      Lexer->Mode = LexModeHashDefineSpaceIdent;
   return TokenIdentifier;
}

// Unescape a character from an octal character constant.
static unsigned char LexUnEscapeCharacterConstant(const char **From, const char *End, unsigned char FirstChar, int Base) {
   unsigned char Total = GET_BASE_DIGIT(FirstChar);
   int CCount;
   for (CCount = 0; IS_BASE_DIGIT(**From, Base) && CCount < 2; CCount++, (*From)++)
      Total = Total*Base + GET_BASE_DIGIT(**From);
   return Total;
}

// Unescape a character from a string or character constant.
static unsigned char LexUnEscapeCharacter(const char **From, const char *End) {
   unsigned char ThisChar;
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
      ThisChar = *(*From)++;
      switch (ThisChar) {
         case '\\':
         return '\\';
         case '\'':
         return '\'';
         case '"':
         return '"';
         case 'a':
         return '\a';
         case 'b':
         return '\b';
         case 'f':
         return '\f';
         case 'n':
         return '\n';
         case 'r':
         return '\r';
         case 't':
         return '\t';
         case 'v':
         return '\v';
         case '0':
         case '1':
         case '2':
         case '3':
         return LexUnEscapeCharacterConstant(From, End, ThisChar, 8);
         case 'x':
         return LexUnEscapeCharacterConstant(From, End, '0', 16);
         default:
         return ThisChar;
      }
   } else
      return *(*From)++;
}

// Get a string constant - used while scanning.
static LexToken LexGetStringConstant(State pc, LexState Lexer, Value Val, char EndChar) {
   bool Escape = false;
   const char *StartPos = Lexer->Pos;
   const char *EndPos;
   char *EscBuf;
   char *EscBufPos;
   char *RegString;
   Value ArrayValue;
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
         Escape = false;
      } else if (*Lexer->Pos == '\\')
         Escape = true;
      LEXER_INC(Lexer);
   }
   EndPos = Lexer->Pos;
   EscBuf = HeapAllocStack(pc, EndPos - StartPos);
   if (EscBuf == NULL)
      LexFail(pc, Lexer, "out of memory");
   for (EscBufPos = EscBuf, Lexer->Pos = StartPos; Lexer->Pos != EndPos;)
      *EscBufPos++ = LexUnEscapeCharacter(&Lexer->Pos, EndPos);
// Try to find an existing copy of this string literal.
   RegString = TableStrRegister2(pc, EscBuf, EscBufPos - EscBuf);
   HeapPopStack(pc, EscBuf, EndPos - StartPos);
   ArrayValue = VariableStringLiteralGet(pc, RegString);
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
      LEXER_INC(Lexer);
   return TokenStringConstant;
}

// Get a character constant - used while scanning.
static LexToken LexGetCharacterConstant(State pc, LexState Lexer, Value Val) {
   Val->Typ = &pc->CharType;
   Val->Val->Character = LexUnEscapeCharacter(&Lexer->Pos, Lexer->End);
   if (Lexer->Pos != Lexer->End && *Lexer->Pos != '\'')
      LexFail(pc, Lexer, "expected \"'\"");
   LEXER_INC(Lexer);
   return TokenCharacterConstant;
}

// Skip a comment - used while scanning.
void LexSkipComment(LexState Lexer, char NextChar, LexToken *ReturnToken) {
   if (NextChar == '*') {
   // Conventional C comment.
      while (Lexer->Pos != Lexer->End && (*(Lexer->Pos - 1) != '*' || *Lexer->Pos != '/')) {
         if (*Lexer->Pos == '\n')
            Lexer->EmitExtraNewlines++;
         LEXER_INC(Lexer);
      }
      if (Lexer->Pos != Lexer->End)
         LEXER_INC(Lexer);
      Lexer->Mode = LexModeNormal;
   } else {
   // C++ style comment.
      while (Lexer->Pos != Lexer->End && *Lexer->Pos != '\n')
         LEXER_INC(Lexer);
   }
}

// Get a single token from the source - used while scanning.
static LexToken LexScanGetToken(State pc, LexState Lexer, Value *ValP) {
   char ThisChar;
   char NextChar;
   LexToken GotToken = TokenNone;
// Handle cases line multi-line comments or string constants which mess up the line count.
   if (Lexer->EmitExtraNewlines > 0) {
      Lexer->EmitExtraNewlines--;
      return TokenEndOfLine;
   }
// Scan for a token.
   do {
      *ValP = &pc->LexValue;
      while (Lexer->Pos != Lexer->End && isspace((int)*Lexer->Pos)) {
         if (*Lexer->Pos == '\n') {
            Lexer->Line++;
            Lexer->Pos++;
            Lexer->Mode = LexModeNormal;
            Lexer->CharacterPos = 0;
            return TokenEndOfLine;
         } else if (Lexer->Mode == LexModeHashDefine || Lexer->Mode == LexModeHashDefineSpace)
            Lexer->Mode = LexModeHashDefineSpace;
         else if (Lexer->Mode == LexModeHashDefineSpaceIdent)
            Lexer->Mode = LexModeNormal;
         LEXER_INC(Lexer);
      }
      if (Lexer->Pos == Lexer->End || *Lexer->Pos == '\0')
         return TokenEOF;
      ThisChar = *Lexer->Pos;
      if (isCidstart((int)ThisChar))
         return LexGetWord(pc, Lexer, *ValP);
      if (isdigit((int)ThisChar))
         return LexGetNumber(pc, Lexer, *ValP);
      NextChar = (Lexer->Pos + 1 != Lexer->End)? *(Lexer->Pos + 1): 0;
      LEXER_INC(Lexer);
      switch (ThisChar) {
         case '"':
            GotToken = LexGetStringConstant(pc, Lexer, *ValP, '"');
         break;
         case '\'':
            GotToken = LexGetCharacterConstant(pc, Lexer, *ValP);
         break;
         case '(':
            if (Lexer->Mode == LexModeHashDefineSpaceIdent) GotToken = TokenOpenMacroBracket;
            else GotToken = TokenOpenBracket;
            Lexer->Mode = LexModeNormal;
         break;
         case ')':
            GotToken = TokenCloseBracket;
         break;
         case '=':
            NEXTIS('=', TokenEqual, TokenAssign);
         break;
         case '+':
            NEXTIS3('=', TokenAddAssign, '+', TokenIncrement, TokenPlus);
         break;
         case '-':
            NEXTIS4('=', TokenSubtractAssign, '>', TokenArrow, '-', TokenDecrement, TokenMinus);
         break;
         case '*':
            NEXTIS('=', TokenMultiplyAssign, TokenAsterisk);
         break;
         case '/':
            if (NextChar == '/' || NextChar == '*') {
               LEXER_INC(Lexer);
               LexSkipComment(Lexer, NextChar, &GotToken);
            } else NEXTIS('=', TokenDivideAssign, TokenSlash);
         break;
         case '%':
            NEXTIS('=', TokenModulusAssign, TokenModulus);
         break;
         case '<':
            if (Lexer->Mode == LexModeHashInclude) GotToken = LexGetStringConstant(pc, Lexer, *ValP, '>');
            else {
               NEXTIS3PLUS('=', TokenLessEqual, '<', TokenShiftLeft, '=', TokenShiftLeftAssign, TokenLessThan);
            }
         break;
         case '>':
            NEXTIS3PLUS('=', TokenGreaterEqual, '>', TokenShiftRight, '=', TokenShiftRightAssign, TokenGreaterThan);
         break;
         case ';':
            GotToken = TokenSemicolon;
         break;
         case '&':
            NEXTIS3('=', TokenArithmeticAndAssign, '&', TokenLogicalAnd, TokenAmpersand);
         break;
         case '|':
            NEXTIS3('=', TokenArithmeticOrAssign, '|', TokenLogicalOr, TokenArithmeticOr);
         break;
         case '{':
            GotToken = TokenLeftBrace;
         break;
         case '}':
            GotToken = TokenRightBrace;
         break;
         case '[':
            GotToken = TokenLeftSquareBracket;
         break;
         case ']':
            GotToken = TokenRightSquareBracket;
         break;
         case '!':
            NEXTIS('=', TokenNotEqual, TokenUnaryNot);
         break;
         case '^':
            NEXTIS('=', TokenArithmeticExorAssign, TokenArithmeticExor);
         break;
         case '~':
            GotToken = TokenUnaryExor;
         break;
         case ',':
            GotToken = TokenComma;
         break;
         case '.':
            NEXTISEXACTLY3('.', '.', TokenEllipsis, TokenDot);
         break;
         case '?':
            GotToken = TokenQuestionMark;
         break;
         case ':':
            GotToken = TokenColon;
         break;
         default:
            LexFail(pc, Lexer, "illegal character '%c'", ThisChar);
         break;
      }
   } while (GotToken == TokenNone);
   return GotToken;
}

// What size value goes with each token.
static int LexTokenSize(LexToken Token) {
   switch (Token) {
      case TokenIdentifier:
      case TokenStringConstant:
      return sizeof(char *);
      case TokenIntegerConstant:
      return sizeof(long);
      case TokenCharacterConstant:
      return sizeof(unsigned char);
      case TokenFPConstant:
      return sizeof(double);
      default:
      return 0;
   }
}

// Produce tokens from the lexer and return a heap buffer with the result - used for scanning.
static void *LexTokenize(State pc, LexState Lexer, int *TokenLen) {
   LexToken Token;
   void *HeapMem;
   Value GotValue;
   int MemUsed = 0;
   int ValueSize;
   int ReserveSpace = (Lexer->End - Lexer->Pos)*4 + 16;
   void *TokenSpace = HeapAllocStack(pc, ReserveSpace);
   char *TokenPos = (char *)TokenSpace;
   int LastCharacterPos = 0;
   if (TokenSpace == NULL)
      LexFail(pc, Lexer, "out of memory");
   do {
   // Store the token at the end of the stack area.
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
      ValueSize = LexTokenSize(Token);
      if (ValueSize > 0) {
      // Store a value as well.
         memcpy((void *)TokenPos, (void *)GotValue->Val, ValueSize);
         TokenPos += ValueSize;
         MemUsed += ValueSize;
      }
      LastCharacterPos = Lexer->CharacterPos;
   } while (Token != TokenEOF);
   HeapMem = HeapAllocMem(pc, MemUsed);
   if (HeapMem == NULL)
      LexFail(pc, Lexer, "out of memory");
   assert(ReserveSpace >= MemUsed);
   memcpy(HeapMem, TokenSpace, MemUsed);
   HeapPopStack(pc, TokenSpace, ReserveSpace);
#ifdef DEBUG_LEXER
{
   int Count;
   printf("Tokens: ");
   for (Count = 0; Count < MemUsed; Count++)
      printf("%02x ", *((unsigned char *)HeapMem + Count));
   printf("\n");
}
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
   Lexer.Mode = LexModeNormal;
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
   Parser->Mode = RunIt? RunModeRun: RunModeSkip;
   Parser->SearchLabel = 0;
   Parser->HashIfLevel = 0;
   Parser->HashIfEvaluateToLevel = 0;
   Parser->CharacterPos = 0;
   Parser->SourceText = SourceText;
   Parser->DebugMode = EnableDebugger;
}

// Get the next token, without pre-processing.
static LexToken LexGetRawToken(ParseState Parser, Value *ValP, int IncPos) {
   LexToken Token = TokenNone;
   int ValueSize;
   char *Prompt = NULL;
   State pc = Parser->pc;
   do {
   // Get the next token.
      if (Parser->Pos == NULL && pc->InteractiveHead != NULL)
         Parser->Pos = pc->InteractiveHead->Tokens;
      if (Parser->FileName != pc->StrEmpty || pc->InteractiveHead != NULL) {
      // Skip leading newlines.
         while ((Token = (LexToken)*(unsigned char *)Parser->Pos) == TokenEndOfLine) {
            Parser->Line++;
            Parser->Pos += TOKEN_DATA_OFFSET;
         }
      }
      if (Parser->FileName == pc->StrEmpty && (pc->InteractiveHead == NULL || Token == TokenEOF)) {
      // We're at the end of an interactive input token list.
         char LineBuffer[LINEBUFFER_MAX];
         void *LineTokens;
         int LineBytes;
         TokenLine LineNode;
         if (pc->InteractiveHead == NULL || (unsigned char *)Parser->Pos == &pc->InteractiveTail->Tokens[pc->InteractiveTail->NumBytes - TOKEN_DATA_OFFSET]) {
         // Get interactive input.
            if (pc->LexUseStatementPrompt) {
               Prompt = INTERACTIVE_PROMPT_STATEMENT;
               pc->LexUseStatementPrompt = false;
            } else
               Prompt = INTERACTIVE_PROMPT_LINE;
            if (PlatformGetLine(&LineBuffer[0], LINEBUFFER_MAX, Prompt) == NULL)
               return TokenEOF;
         // Put the new line at the end of the linked list of interactive lines.
            LineTokens = LexAnalyse(pc, pc->StrEmpty, &LineBuffer[0], strlen(LineBuffer), &LineBytes);
            LineNode = VariableAlloc(pc, Parser, sizeof *LineNode, true);
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
            if (Parser->Pos != &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes - TOKEN_DATA_OFFSET]) {
            // Scan for the line.
               for (pc->InteractiveCurrentLine = pc->InteractiveHead; Parser->Pos != &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes - TOKEN_DATA_OFFSET]; pc->InteractiveCurrentLine = pc->InteractiveCurrentLine->Next) {
                  assert(pc->InteractiveCurrentLine->Next != NULL);
               }
            }
            assert(pc->InteractiveCurrentLine != NULL);
            pc->InteractiveCurrentLine = pc->InteractiveCurrentLine->Next;
            assert(pc->InteractiveCurrentLine != NULL);
            Parser->Pos = pc->InteractiveCurrentLine->Tokens;
         }
         Token = (LexToken)*(unsigned char *)Parser->Pos;
      }
   } while ((Parser->FileName == pc->StrEmpty && Token == TokenEOF) || Token == TokenEndOfLine);
   Parser->CharacterPos = *((unsigned char *)Parser->Pos + 1);
   ValueSize = LexTokenSize(Token);
   if (ValueSize > 0) {
   // This token requires a value - unpack it.
      if (ValP != NULL) {
         switch (Token) {
            case TokenStringConstant:
               pc->LexValue.Typ = pc->CharPtrType;
            break;
            case TokenIdentifier:
               pc->LexValue.Typ = NULL;
            break;
            case TokenIntegerConstant:
               pc->LexValue.Typ = &pc->LongType;
            break;
            case TokenCharacterConstant:
               pc->LexValue.Typ = &pc->CharType;
            break;
#ifndef NO_FP
            case TokenFPConstant:
               pc->LexValue.Typ = &pc->FPType;
            break;
#endif
            default:
            break;
         }
         memcpy((void *)pc->LexValue.Val, (void *)((char *)Parser->Pos + TOKEN_DATA_OFFSET), ValueSize);
         pc->LexValue.ValOnHeap = false;
         pc->LexValue.ValOnStack = false;
         pc->LexValue.IsLValue = false;
         pc->LexValue.LValueFrom = NULL;
         *ValP = &pc->LexValue;
      }
      if (IncPos)
         Parser->Pos += ValueSize + TOKEN_DATA_OFFSET;
   } else {
      if (IncPos && Token != TokenEOF)
         Parser->Pos += TOKEN_DATA_OFFSET;
   }
#ifdef DEBUG_LEXER
   printf("Got token=%02x inc=%d pos=%d\n", Token, IncPos, Parser->CharacterPos);
#endif
   assert(Token >= TokenNone && Token <= TokenEndOfFunction);
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
   Value SavedValue;
   bool IsDefined;
   LexToken Token = LexGetRawToken(Parser, &IdentValue, true);
   if (Token != TokenIdentifier)
      ProgramFail(Parser, "identifier expected");
// Is the identifier defined?
   IsDefined = TableGet(&Parser->pc->GlobalTable, IdentValue->Val->Identifier, &SavedValue, NULL, NULL, NULL);
   if (Parser->HashIfEvaluateToLevel == Parser->HashIfLevel && ((IsDefined && !IfNot) || (!IsDefined && IfNot))) {
   // #if is active, evaluate to this new level.
      Parser->HashIfEvaluateToLevel++;
   }
   Parser->HashIfLevel++;
}

// Handle a #if directive.
static void LexHashIf(ParseState Parser) {
// Get symbol to check.
   Value IdentValue;
   Value SavedValue = NULL;
   struct ParseState MacroParser;
   LexToken Token = LexGetRawToken(Parser, &IdentValue, true);
   if (Token == TokenIdentifier) {
   // Look up a value from a macro definition.
      if (!TableGet(&Parser->pc->GlobalTable, IdentValue->Val->Identifier, &SavedValue, NULL, NULL, NULL))
         ProgramFail(Parser, "'%s' is undefined", IdentValue->Val->Identifier);
      if (SavedValue->Typ->Base != TypeMacro)
         ProgramFail(Parser, "value expected");
      ParserCopy(&MacroParser, &SavedValue->Val->MacroDef.Body);
      Token = LexGetRawToken(&MacroParser, &IdentValue, true);
   }
   if (Token != TokenCharacterConstant && Token != TokenIntegerConstant)
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
void LexPrintToken(LexToken Token) {
   char *TokenNames[] = {
   /*0x00*/ "None",
   /*0x01*/ "Comma",
   /*0x02*/ "Assign", "AddAssign", "SubtractAssign", "MultiplyAssign", "DivideAssign", "ModulusAssign",
   /*0x08*/ "ShiftLeftAssign", "ShiftRightAssign", "ArithmeticAndAssign", "ArithmeticOrAssign", "ArithmeticExorAssign",
   /*0x0d*/ "QuestionMark", "Colon",
   /*0x0f*/ "LogicalOr",
   /*0x10*/ "LogicalAnd",
   /*0x11*/ "ArithmeticOr",
   /*0x12*/ "ArithmeticExor",
   /*0x13*/ "Ampersand",
   /*0x14*/ "Equal", "NotEqual",
   /*0x16*/ "LessThan", "GreaterThan", "LessEqual", "GreaterEqual",
   /*0x1a*/ "ShiftLeft", "ShiftRight",
   /*0x1c*/ "Plus", "Minus",
   /*0x1e*/ "Asterisk", "Slash", "Modulus",
   /*0x21*/ "Increment", "Decrement", "UnaryNot", "UnaryExor", "Sizeof", "Cast",
   /*0x27*/ "LeftSquareBracket", "RightSquareBracket", "Dot", "Arrow",
   /*0x2b*/ "OpenBracket", "CloseBracket",
   /*0x2d*/ "Identifier", "IntegerConstant", "FPConstant", "StringConstant", "CharacterConstant",
   /*0x32*/ "Semicolon", "Ellipsis",
   /*0x34*/ "LeftBrace", "RightBrace",
   /*0x36*/ "IntType", "CharType", "FloatType", "DoubleType", "VoidType", "EnumType",
   /*0x3c*/ "LongType", "SignedType", "ShortType", "StaticType", "AutoType", "RegisterType", "ExternType", "StructType", "UnionType", "UnsignedType", "Typedef",
   /*0x46*/ "Continue", "Do", "Else", "For", "Goto", "If", "While", "Break", "Switch", "Case", "Default", "Return",
   /*0x52*/ "HashDefine", "HashInclude", "HashIf", "HashIfdef", "HashIfndef", "HashElse", "HashEndif",
   /*0x59*/ "New", "Delete",
   /*0x5b*/ "OpenMacroBracket",
   /*0x5c*/ "EOF", "EndOfLine", "EndOfFunction"
   };
   printf("{%s}", TokenNames[Token]);
}
#endif

// Get the next token given a parser state, pre-processing as we go.
LexToken LexGetToken(ParseState Parser, Value *ValP, int IncPos) {
   LexToken Token;
   bool TryNextToken;
// Implements the pre-processor #if commands.
   do {
      int WasPreProcToken = true;
      Token = LexGetRawToken(Parser, ValP, IncPos);
      switch (Token) {
         case TokenHashIfdef:
            LexHashIncPos(Parser, IncPos);
            LexHashIfdef(Parser, false);
         break;
         case TokenHashIfndef:
            LexHashIncPos(Parser, IncPos);
            LexHashIfdef(Parser, true);
         break;
         case TokenHashIf:
            LexHashIncPos(Parser, IncPos);
            LexHashIf(Parser);
         break;
         case TokenHashElse:
            LexHashIncPos(Parser, IncPos);
            LexHashElse(Parser);
         break;
         case TokenHashEndif:
            LexHashIncPos(Parser, IncPos);
            LexHashEndif(Parser);
         break;
         default:
            WasPreProcToken = false;
         break;
      }
   // If we're going to reject this token, increment the token pointer to the next one.
      TryNextToken = (Parser->HashIfEvaluateToLevel < Parser->HashIfLevel && Token != TokenEOF) || WasPreProcToken;
      if (!IncPos && TryNextToken)
         LexGetRawToken(Parser, NULL, true);
   } while (TryNextToken);
   return Token;
}

// Take a quick peek at the next token, skipping any pre-processing.
LexToken LexRawPeekToken(ParseState Parser) {
   return (LexToken)*(unsigned char *)Parser->Pos;
}

// Find the end of the line.
void LexToEndOfLine(ParseState Parser) {
   while (true) {
      LexToken Token = (LexToken)*(unsigned char *)Parser->Pos;
      if (Token == TokenEndOfLine || Token == TokenEOF)
         return;
      else
         LexGetRawToken(Parser, NULL, true);
   }
}

// Copy the tokens from StartParser to EndParser into new memory, removing TokenEOFs and terminate with a TokenEndOfFunction.
void *LexCopyTokens(ParseState StartParser, ParseState EndParser) {
   int MemSize = 0;
   int CopySize;
   unsigned char *Pos = (unsigned char *)StartParser->Pos;
   unsigned char *NewTokens;
   unsigned char *NewTokenPos;
   TokenLine ILine;
   State pc = StartParser->pc;
   if (pc->InteractiveHead == NULL) {
   // Non-interactive mode - copy the tokens.
      MemSize = EndParser->Pos - StartParser->Pos;
      NewTokens = VariableAlloc(pc, StartParser, MemSize + TOKEN_DATA_OFFSET, true);
      memcpy(NewTokens, (void *)StartParser->Pos, MemSize);
   } else {
   // We're in interactive mode - add up line by line.
      for (pc->InteractiveCurrentLine = pc->InteractiveHead; pc->InteractiveCurrentLine != NULL && (Pos < &pc->InteractiveCurrentLine->Tokens[0] || Pos >= &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes]); pc->InteractiveCurrentLine = pc->InteractiveCurrentLine->Next) {
      } // Find the line we just counted.
      if (EndParser->Pos >= StartParser->Pos && EndParser->Pos < &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes]) {
      // All on a single line.
         MemSize = EndParser->Pos - StartParser->Pos;
         NewTokens = VariableAlloc(pc, StartParser, MemSize + TOKEN_DATA_OFFSET, true);
         memcpy(NewTokens, (void *)StartParser->Pos, MemSize);
      } else {
      // It's spread across multiple lines.
         MemSize = &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes - TOKEN_DATA_OFFSET] - Pos;
         for (ILine = pc->InteractiveCurrentLine->Next; ILine != NULL && (EndParser->Pos < &ILine->Tokens[0] || EndParser->Pos >= &ILine->Tokens[ILine->NumBytes]); ILine = ILine->Next)
            MemSize += ILine->NumBytes - TOKEN_DATA_OFFSET;
         assert(ILine != NULL);
         MemSize += EndParser->Pos - &ILine->Tokens[0];
         NewTokens = VariableAlloc(pc, StartParser, MemSize + TOKEN_DATA_OFFSET, true);
         CopySize = &pc->InteractiveCurrentLine->Tokens[pc->InteractiveCurrentLine->NumBytes - TOKEN_DATA_OFFSET] - Pos;
         memcpy(NewTokens, Pos, CopySize);
         NewTokenPos = NewTokens + CopySize;
         for (ILine = pc->InteractiveCurrentLine->Next; ILine != NULL && (EndParser->Pos < &ILine->Tokens[0] || EndParser->Pos >= &ILine->Tokens[ILine->NumBytes]); ILine = ILine->Next) {
            memcpy(NewTokenPos, &ILine->Tokens[0], ILine->NumBytes - TOKEN_DATA_OFFSET);
            NewTokenPos += ILine->NumBytes - TOKEN_DATA_OFFSET;
         }
         assert(ILine != NULL);
         memcpy(NewTokenPos, &ILine->Tokens[0], EndParser->Pos - &ILine->Tokens[0]);
      }
   }
   NewTokens[MemSize] = (unsigned char)TokenEndOfFunction;
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
   while (pc->InteractiveHead != NULL && !(Parser->Pos >= &pc->InteractiveHead->Tokens[0] && Parser->Pos < &pc->InteractiveHead->Tokens[pc->InteractiveHead->NumBytes])) {
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
