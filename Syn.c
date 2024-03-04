// PicoC parser
// Parses source and executes statements.
#include "Main.h"
#include "Extern.h"

// Deallocate any memory.
void ParseCleanup(State pc) {
   while (pc->CleanupTokenList != NULL) {
      CleanupTokenNode Next = pc->CleanupTokenList->Next;
      HeapFreeMem(pc, pc->CleanupTokenList->Tokens);
      if (pc->CleanupTokenList->SourceText != NULL)
         HeapFreeMem(pc, (void *)pc->CleanupTokenList->SourceText);
      HeapFreeMem(pc, pc->CleanupTokenList);
      pc->CleanupTokenList = Next;
   }
}

// Parse a statement, but only run it if Condition is true.
static ParseResult ParseStatementMaybeRun(ParseState Parser, bool Condition, bool CheckTrailingSemicolon) {
   RunMode OldMode = Parser->Mode;
   bool DoSkip = OldMode != SkipM && !Condition;
   if (DoSkip) Parser->Mode = SkipM;
   int Result = ParseStatement(Parser, CheckTrailingSemicolon);
   if (DoSkip) Parser->Mode = OldMode;
   return Result;
}

// Count the number of parameters to a function or macro.
static int ParseCountParams(ParseState Parser) {
   int ParamCount = 0;
   Lexical Token = LexGetToken(Parser, NULL, true);
   if (Token != RParL && Token != EofL) {
   // Count the number of parameters.
      ParamCount++;
      while ((Token = LexGetToken(Parser, NULL, true)) != RParL && Token != EofL) {
         if (Token == CommaL)
            ParamCount++;
      }
   }
   return ParamCount;
}

// Parse a function definition and store it for later.
Value ParseFunctionDefinition(ParseState Parser, ValueType ReturnType, char *Identifier) {
   State pc = Parser->pc;
   if (pc->TopStackFrame != NULL)
      ProgramFail(Parser, "nested function definitions are not allowed");
   LexGetToken(Parser, NULL, true); // Open bracket.
   struct ParseState ParamParser;
   ParserCopy(&ParamParser, Parser);
   int ParamCount = ParseCountParams(Parser);
   if (ParamCount > ParameterMax)
      ProgramFail(Parser, "too many parameters (%d allowed)", ParameterMax);
   Value FuncValue = VariableAllocValueAndData(pc, Parser, sizeof FuncValue->Val->FuncDef + ParamCount*(sizeof(ValueType) + sizeof(const char *)), false, NULL, true);
   FuncValue->Typ = &pc->FunctionType;
   FuncValue->Val->FuncDef.ReturnType = ReturnType;
   FuncValue->Val->FuncDef.NumParams = ParamCount;
   FuncValue->Val->FuncDef.VarArgs = false;
   FuncValue->Val->FuncDef.ParamType = (ValueType *)((char *)FuncValue->Val + sizeof FuncValue->Val->FuncDef);
   FuncValue->Val->FuncDef.ParamName = (char **)((char *)FuncValue->Val->FuncDef.ParamType + ParamCount*sizeof(ValueType));
   Lexical Token = NoneL;
   for (ParamCount = 0; ParamCount < FuncValue->Val->FuncDef.NumParams; ParamCount++) {
   // Harvest the parameters into the function definition.
      if (ParamCount == FuncValue->Val->FuncDef.NumParams - 1 && LexGetToken(&ParamParser, NULL, false) == DotsL) {
      // Ellipsis at end.
         FuncValue->Val->FuncDef.NumParams--;
         FuncValue->Val->FuncDef.VarArgs = true;
         break;
      } else {
      // Add a parameter.
         ValueType ParamType; char *ParamIdentifier;
         TypeParse(&ParamParser, &ParamType, &ParamIdentifier, NULL);
         if (ParamType->Base == VoidT) {
         // This isn't a real parameter at all - delete it.
            ParamCount--;
            FuncValue->Val->FuncDef.NumParams--;
         } else {
            FuncValue->Val->FuncDef.ParamType[ParamCount] = ParamType;
            FuncValue->Val->FuncDef.ParamName[ParamCount] = ParamIdentifier;
         }
      }
      Token = LexGetToken(&ParamParser, NULL, true);
      if (Token != CommaL && ParamCount < FuncValue->Val->FuncDef.NumParams - 1)
         ProgramFail(&ParamParser, "comma expected");
   }
   if (FuncValue->Val->FuncDef.NumParams != 0 && Token != RParL && Token != CommaL && Token != DotsL)
      ProgramFail(&ParamParser, "bad parameter");
   if (strcmp(Identifier, "main") == 0) {
   // Make sure it's int main().
      if (FuncValue->Val->FuncDef.ReturnType != &pc->IntType && FuncValue->Val->FuncDef.ReturnType != &pc->VoidType)
         ProgramFail(Parser, "main() should return an int or void");
      if (FuncValue->Val->FuncDef.NumParams != 0 && (FuncValue->Val->FuncDef.NumParams != 2 || FuncValue->Val->FuncDef.ParamType[0] != &pc->IntType))
         ProgramFail(Parser, "bad parameters to main()");
   }
// Look for a function body.
   Token = LexGetToken(Parser, NULL, false);
   if (Token == SemiL)
      LexGetToken(Parser, NULL, true); // It's a prototype, absorb the trailing semicolon.
   else {
   // It's a full function definition with a body.
      if (Token != LCurlL)
         ProgramFail(Parser, "bad function definition");
      struct ParseState FuncBody;
      ParserCopy(&FuncBody, Parser);
      if (ParseStatementMaybeRun(Parser, false, true) != OkSyn)
         ProgramFail(Parser, "function definition expected");
      FuncValue->Val->FuncDef.Body = FuncBody;
      FuncValue->Val->FuncDef.Body.Pos = LexCopyTokens(&FuncBody, Parser);
   // Is this function already in the global table?
      Value OldFuncValue;
      if (TableGet(&pc->GlobalTable, Identifier, &OldFuncValue, NULL, NULL, NULL)) {
         if (OldFuncValue->Val->FuncDef.Body.Pos == NULL) {
         // Override an old function prototype.
            VariableFree(pc, TableDelete(pc, &pc->GlobalTable, Identifier));
         } else
            ProgramFail(Parser, "'%s' is already defined", Identifier);
      }
   }
   if (!TableSet(pc, &pc->GlobalTable, Identifier, FuncValue, (char *)Parser->FileName, Parser->Line, Parser->CharacterPos))
      ProgramFail(Parser, "'%s' is already defined", Identifier);
   return FuncValue;
}

// Parse an array initializer and assign to a variable.
static int ParseArrayInitializer(ParseState Parser, Value NewVariable, bool DoAssignment) {
// Count the number of elements in the array.
   if (DoAssignment && Parser->Mode == RunM) {
      struct ParseState CountParser;
      ParserCopy(&CountParser, Parser);
      int NumElements = ParseArrayInitializer(&CountParser, NewVariable, false);
      if (NewVariable->Typ->Base != ArrayT)
         AssignFail(Parser, "%t from array initializer", NewVariable->Typ, NULL, 0, 0, NULL, 0);
      if (NewVariable->Typ->ArraySize == 0) {
         NewVariable->Typ = TypeGetMatching(Parser->pc, Parser, NewVariable->Typ->FromType, NewVariable->Typ->Base, NumElements, NewVariable->Typ->Identifier, true);
         VariableRealloc(Parser, NewVariable, TypeSizeValue(NewVariable, false));
      }
#ifdef DEBUG_ARRAY_INITIALIZER
      ShowSourcePos(Parser);
      printf("array size: %d \n", NewVariable->Typ->ArraySize);
#endif
   }
// Parse the array initializer.
   int ArrayIndex = 0;
   Lexical Token = LexGetToken(Parser, NULL, false);
   while (Token != RCurlL) {
      if (LexGetToken(Parser, NULL, false) == LCurlL) {
      // This is a sub-array initializer.
         Value SubArray = NewVariable;
         if (Parser->Mode == RunM && DoAssignment) {
            int SubArraySize = TypeSize(NewVariable->Typ->FromType, NewVariable->Typ->FromType->ArraySize, true);
            SubArray = VariableAllocValueFromExistingData(Parser, NewVariable->Typ->FromType, (AnyValue)(NewVariable->Val->ArrayMem + SubArraySize*ArrayIndex), true, NewVariable);
#ifdef DEBUG_ARRAY_INITIALIZER
            int FullArraySize = TypeSize(NewVariable->Typ, NewVariable->Typ->ArraySize, true);
            ShowSourcePos(Parser);
            ShowType(Parser, NewVariable->Typ);
            printf("[%d] subarray size: %d (full: %d,%d) \n", ArrayIndex, SubArraySize, FullArraySize, NewVariable->Typ->ArraySize);
#endif
            if (ArrayIndex >= NewVariable->Typ->ArraySize)
               ProgramFail(Parser, "too many array elements");
         }
         LexGetToken(Parser, NULL, true);
         ParseArrayInitializer(Parser, SubArray, DoAssignment);
      } else {
         Value ArrayElement = NULL;
         if (Parser->Mode == RunM && DoAssignment) {
            ValueType ElementType = NewVariable->Typ;
            int TotalSize = 1;
         // int x[3][3] = {1,2,3,4} => handle it just like int x[9] = {1,2,3,4}.
            while (ElementType->Base == ArrayT) {
               TotalSize *= ElementType->ArraySize;
               ElementType = ElementType->FromType;
            // char x[10][10] = {"abc", "def"} => assign "abc" to x[0], "def" to x[1] etc.
               if (LexGetToken(Parser, NULL, false) == StrLitL && ElementType->FromType->Base == CharT)
                  break;
            }
            int ElementSize = TypeSize(ElementType, ElementType->ArraySize, true);
#ifdef DEBUG_ARRAY_INITIALIZER
            ShowSourcePos(Parser);
            printf("[%d/%d] element size: %d (x%d) \n", ArrayIndex, TotalSize, ElementSize, ElementType->ArraySize);
#endif
            if (ArrayIndex >= TotalSize)
               ProgramFail(Parser, "too many array elements");
            ArrayElement = VariableAllocValueFromExistingData(Parser, ElementType, (AnyValue)(NewVariable->Val->ArrayMem + ElementSize*ArrayIndex), true, NewVariable);
         }
      // This is a normal expression initializer.
         Value CValue;
         if (!ExpressionParse(Parser, &CValue))
            ProgramFail(Parser, "expression expected");
         if (Parser->Mode == RunM && DoAssignment) {
            ExpressionAssign(Parser, ArrayElement, CValue, false, NULL, 0, false);
            VariableStackPop(Parser, CValue);
            VariableStackPop(Parser, ArrayElement);
         }
      }
      ArrayIndex++;
      Token = LexGetToken(Parser, NULL, false);
      if (Token == CommaL) {
         LexGetToken(Parser, NULL, true);
         Token = LexGetToken(Parser, NULL, false);
      } else if (Token != RCurlL)
         ProgramFail(Parser, "comma expected");
   }
   if (Token == RCurlL)
      LexGetToken(Parser, NULL, true);
   else
      ProgramFail(Parser, "'}' expected");
   return ArrayIndex;
}

// Assign an initial value to a variable.
static void ParseDeclarationAssignment(ParseState Parser, Value NewVariable, bool DoAssignment) {
   if (LexGetToken(Parser, NULL, false) == LCurlL) {
   // This is an array initializer.
      LexGetToken(Parser, NULL, true);
      ParseArrayInitializer(Parser, NewVariable, DoAssignment);
   } else {
   // This is a normal expression initializer.
      Value CValue;
      if (!ExpressionParse(Parser, &CValue))
         ProgramFail(Parser, "expression expected");
      if (Parser->Mode == RunM && DoAssignment) {
         ExpressionAssign(Parser, NewVariable, CValue, false, NULL, 0, false);
         VariableStackPop(Parser, CValue);
      }
   }
}

// Declare a variable or function.
static bool ParseDeclaration(ParseState Parser, Lexical Token) {
   State pc = Parser->pc;
   ValueType BasicType; bool IsStatic = false;
   TypeParseFront(Parser, &BasicType, &IsStatic);
   Value NewVariable = NULL;
   bool FirstVisit = false;
   do {
      ValueType Typ; char *Identifier;
      TypeParseIdentPart(Parser, BasicType, &Typ, &Identifier);
      if (Token != VoidL && Token != StructL && Token != UnionL && Token != EnumL && Identifier == pc->StrEmpty)
         ProgramFail(Parser, "identifier expected");
      if (Identifier != pc->StrEmpty) {
      // Handle function definitions.
         if (LexGetToken(Parser, NULL, false) == LParL) {
            ParseFunctionDefinition(Parser, Typ, Identifier);
            return false;
         } else {
            if (Typ == &pc->VoidType && Identifier != pc->StrEmpty)
               ProgramFail(Parser, "can't define a void variable");
            if (Parser->Mode == RunM || Parser->Mode == GotoM)
               NewVariable = VariableDefineButIgnoreIdentical(Parser, Identifier, Typ, IsStatic, &FirstVisit);
            if (LexGetToken(Parser, NULL, false) == EquL) {
            // We're assigning an initial value.
               LexGetToken(Parser, NULL, true);
               ParseDeclarationAssignment(Parser, NewVariable, !IsStatic || FirstVisit);
            }
         }
      }
      Token = LexGetToken(Parser, NULL, false);
      if (Token == CommaL)
         LexGetToken(Parser, NULL, true);
   } while (Token == CommaL);
   return true;
}

// Parse a #define macro definition and store it for later.
static void ParseMacroDefinition(ParseState Parser) {
   Value MacroName;
   if (LexGetToken(Parser, &MacroName, true) != IdL)
      ProgramFail(Parser, "identifier expected");
   char *MacroNameStr = MacroName->Val->Identifier;
   Value MacroValue;
   if (LexRawPeekToken(Parser) == LParP) {
   // It's a parameterized macro, read the parameters.
      Lexical Token = LexGetToken(Parser, NULL, true);
      struct ParseState ParamParser;
      ParserCopy(&ParamParser, Parser);
      int NumParams = ParseCountParams(&ParamParser);
      MacroValue = VariableAllocValueAndData(Parser->pc, Parser, sizeof MacroValue->Val->MacroDef + NumParams*sizeof(const char *), false, NULL, true);
      MacroValue->Val->MacroDef.NumParams = NumParams;
      MacroValue->Val->MacroDef.ParamName = (char **)((char *)MacroValue->Val + sizeof MacroValue->Val->MacroDef);
      Value ParamName;
      Token = LexGetToken(Parser, &ParamName, true);
      int ParamCount = 0;
      while (Token == IdL) {
      // Store a parameter name.
         MacroValue->Val->MacroDef.ParamName[ParamCount++] = ParamName->Val->Identifier;
      // Get the trailing comma.
         Token = LexGetToken(Parser, NULL, true);
         if (Token == CommaL)
            Token = LexGetToken(Parser, &ParamName, true);
         else if (Token != RParL)
            ProgramFail(Parser, "comma expected");
      }
      if (Token != RParL)
         ProgramFail(Parser, "close bracket expected");
   } else {
   // Allocate a simple unparameterized macro.
      MacroValue = VariableAllocValueAndData(Parser->pc, Parser, sizeof MacroValue->Val->MacroDef, false, NULL, true);
      MacroValue->Val->MacroDef.NumParams = 0;
   }
// Copy the body of the macro to execute later.
   ParserCopy(&MacroValue->Val->MacroDef.Body, Parser);
   MacroValue->Typ = &Parser->pc->MacroType;
   LexToEndOfLine(Parser);
   MacroValue->Val->MacroDef.Body.Pos = LexCopyTokens(&MacroValue->Val->MacroDef.Body, Parser);
   if (!TableSet(Parser->pc, &Parser->pc->GlobalTable, MacroNameStr, MacroValue, (char *)Parser->FileName, Parser->Line, Parser->CharacterPos))
      ProgramFail(Parser, "'%s' is already defined", MacroNameStr);
}

// Copy the entire parser state.
void ParserCopy(ParseState To, ParseState From) {
   memcpy((void *)To, (void *)From, sizeof *To);
}

// Copy where we're at in the parsing.
static void ParserCopyPos(ParseState To, ParseState From) {
   To->Pos = From->Pos;
   To->Line = From->Line;
   To->HashIfLevel = From->HashIfLevel;
   To->HashIfEvaluateToLevel = From->HashIfEvaluateToLevel;
   To->CharacterPos = From->CharacterPos;
}

// Parse a "for" statement.
static void ParseFor(ParseState Parser) {
   RunMode OldMode = Parser->Mode;
   int PrevScopeID = 0, ScopeID = VariableScopeBegin(Parser, &PrevScopeID);
   if (LexGetToken(Parser, NULL, true) != LParL)
      ProgramFail(Parser, "'(' expected");
   if (ParseStatement(Parser, true) != OkSyn)
      ProgramFail(Parser, "statement expected");
   struct ParseState PreConditional;
   ParserCopyPos(&PreConditional, Parser);
   bool Condition = LexGetToken(Parser, NULL, false) == SemiL || ExpressionParseInt(Parser) != 0;
   if (LexGetToken(Parser, NULL, true) != SemiL)
      ProgramFail(Parser, "';' expected");
   struct ParseState PreIncrement;
   ParserCopyPos(&PreIncrement, Parser);
   ParseStatementMaybeRun(Parser, false, false);
   if (LexGetToken(Parser, NULL, true) != RParL)
      ProgramFail(Parser, "')' expected");
   struct ParseState PreStatement;
   ParserCopyPos(&PreStatement, Parser);
   if (ParseStatementMaybeRun(Parser, Condition, true) != OkSyn)
      ProgramFail(Parser, "statement expected");
   if (Parser->Mode == ContinueM && OldMode == RunM)
      Parser->Mode = RunM;
   struct ParseState After;
   ParserCopyPos(&After, Parser);
   while (Condition && Parser->Mode == RunM) {
      ParserCopyPos(Parser, &PreIncrement);
      ParseStatement(Parser, false);
      ParserCopyPos(Parser, &PreConditional);
      Condition = LexGetToken(Parser, NULL, false) == SemiL || ExpressionParseInt(Parser) != 0;
      if (Condition) {
         ParserCopyPos(Parser, &PreStatement);
         ParseStatement(Parser, true);
         if (Parser->Mode == ContinueM)
            Parser->Mode = RunM;
      }
   }
   if (Parser->Mode == BreakM && OldMode == RunM)
      Parser->Mode = RunM;
   VariableScopeEnd(Parser, ScopeID, PrevScopeID);
   ParserCopyPos(Parser, &After);
}

// Parse a block of code and return what mode it returned in.
static RunMode ParseBlock(ParseState Parser, bool AbsorbOpenBrace, bool Condition) {
   int PrevScopeID = 0, ScopeID = VariableScopeBegin(Parser, &PrevScopeID);
   if (AbsorbOpenBrace && LexGetToken(Parser, NULL, true) != LCurlL)
      ProgramFail(Parser, "'{' expected");
   if (Parser->Mode == SkipM || !Condition) {
   // Condition failed - skip this block instead.
      RunMode OldMode = Parser->Mode;
      Parser->Mode = SkipM;
      while (ParseStatement(Parser, true) == OkSyn) {
      }
      Parser->Mode = OldMode;
   } else {
   // Just run it in its current mode.
      while (ParseStatement(Parser, true) == OkSyn) {
      }
   }
   if (LexGetToken(Parser, NULL, true) != RCurlL)
      ProgramFail(Parser, "'}' expected");
   VariableScopeEnd(Parser, ScopeID, PrevScopeID);
   return Parser->Mode;
}

// Parse a typedef declaration.
static void ParseTypedef(ParseState Parser) {
   ValueType Typ; char *TypeName;
   TypeParse(Parser, &Typ, &TypeName, NULL);
   if (Parser->Mode == RunM) {
      ValueType *TypPtr = &Typ;
      struct Value InitValue;
      InitValue.Typ = &Parser->pc->TypeType;
      InitValue.Val = (AnyValue)TypPtr;
      VariableDefine(Parser->pc, Parser, TypeName, &InitValue, NULL, false);
   }
}

// Parse a statement.
ParseResult ParseStatement(ParseState Parser, bool CheckTrailingSemicolon) {
// If we're debugging, check for a breakpoint.
   if (Parser->DebugMode && Parser->Mode == RunM)
      DebugCheckStatement(Parser);
// Take note of where we are and then grab a token to see what statement we have.
   struct ParseState PreState;
   ParserCopy(&PreState, Parser);
   Value LexerValue;
   Lexical Token = LexGetToken(Parser, &LexerValue, true);
   switch (Token) {
      case EofL: return EofSyn;
      case IdL:
      // Might be a typedef-typed variable declaration or it might be an expression.
         if (VariableDefined(Parser->pc, LexerValue->Val->Identifier)) {
            Value VarValue;
            VariableGet(Parser->pc, Parser, LexerValue->Val->Identifier, &VarValue);
            if (VarValue->Typ->Base == TypeT) {
               *Parser = PreState;
               ParseDeclaration(Parser, Token);
               break;
            }
         } else {
         // It might be a goto label.
            Lexical NextToken = LexGetToken(Parser, NULL, false);
            if (NextToken == ColonL) {
            // Declare the identifier as a goto label.
               LexGetToken(Parser, NULL, true);
               if (Parser->Mode == GotoM && LexerValue->Val->Identifier == Parser->SearchGotoLabel)
                  Parser->Mode = RunM;
               CheckTrailingSemicolon = false;
               break;
            }
#ifdef FEATURE_AUTO_DECLARE_VARIABLES
            else { // New_identifier = something.
         // Try to guess type and declare the variable based on assigned value.
               if (NextToken == EquL && !VariableDefinedAndOutOfScope(Parser->pc, LexerValue->Val->Identifier)) {
                  if (Parser->Mode == RunM) {
                     char *Identifier = LexerValue->Val->Identifier;
                     LexGetToken(Parser, NULL, true);
                     Value CValue;
                     if (!ExpressionParse(Parser, &CValue)) {
                        ProgramFail(Parser, "expected: expression");
                     }
#   if 0
                     ShowSourcePos(Parser);
                     PlatformPrintf(Parser->pc->CStdOut, "%t %s = %d;\n", CValue->Typ, Identifier, CValue->Val->Integer);
                     printf("%d\n", VariableDefined(Parser->pc, Identifier));
#   endif
                     VariableDefine(Parser->pc, Parser, Identifier, CValue, CValue->Typ, true);
                     break;
                  }
               }
            }
#endif
         }
      case StarL: case AndL: case IncOpL: case DecOpL: case LParL: {
         *Parser = PreState;
         Value CValue;
         ExpressionParse(Parser, &CValue);
         if (Parser->Mode == RunM)
            VariableStackPop(Parser, CValue);
      }
      break;
      case LCurlL: ParseBlock(Parser, false, true), CheckTrailingSemicolon = false; break;
      case IfL: {
         if (LexGetToken(Parser, NULL, true) != LParL)
            ProgramFail(Parser, "'(' expected");
         bool Condition = ExpressionParseInt(Parser) != 0;
         if (LexGetToken(Parser, NULL, true) != RParL)
            ProgramFail(Parser, "')' expected");
         if (ParseStatementMaybeRun(Parser, Condition, true) != OkSyn)
            ProgramFail(Parser, "statement expected");
         if (LexGetToken(Parser, NULL, false) == ElseL) {
            LexGetToken(Parser, NULL, true);
            if (ParseStatementMaybeRun(Parser, !Condition, true) != OkSyn)
               ProgramFail(Parser, "statement expected");
         }
         CheckTrailingSemicolon = false;
      }
      break;
      case WhileL: {
         RunMode PreMode = Parser->Mode;
         if (LexGetToken(Parser, NULL, true) != LParL)
            ProgramFail(Parser, "'(' expected");
         struct ParseState PreConditional;
         ParserCopyPos(&PreConditional, Parser);
         bool Condition;
         do {
            ParserCopyPos(Parser, &PreConditional);
            Condition = ExpressionParseInt(Parser) != 0;
            if (LexGetToken(Parser, NULL, true) != RParL)
               ProgramFail(Parser, "')' expected");
            if (ParseStatementMaybeRun(Parser, Condition, true) != OkSyn)
               ProgramFail(Parser, "statement expected");
            if (Parser->Mode == ContinueM)
               Parser->Mode = PreMode;
         } while (Parser->Mode == RunM && Condition);
         if (Parser->Mode == BreakM)
            Parser->Mode = PreMode;
         CheckTrailingSemicolon = false;
      }
      break;
      case DoL: {
         RunMode PreMode = Parser->Mode;
         struct ParseState PreStatement;
         ParserCopyPos(&PreStatement, Parser);
         bool Condition;
         do {
            ParserCopyPos(Parser, &PreStatement);
            if (ParseStatement(Parser, true) != OkSyn)
               ProgramFail(Parser, "statement expected");
            if (Parser->Mode == ContinueM)
               Parser->Mode = PreMode;
            if (LexGetToken(Parser, NULL, true) != WhileL)
               ProgramFail(Parser, "'while' expected");
            if (LexGetToken(Parser, NULL, true) != LParL)
               ProgramFail(Parser, "'(' expected");
            Condition = ExpressionParseInt(Parser) != 0;
            if (LexGetToken(Parser, NULL, true) != RParL)
               ProgramFail(Parser, "')' expected");
         } while (Condition && Parser->Mode == RunM);
         if (Parser->Mode == BreakM)
            Parser->Mode = PreMode;
      }
      break;
      case ForL: ParseFor(Parser), CheckTrailingSemicolon = false; break;
      case SemiL: CheckTrailingSemicolon = false; break;
      case IntL: case ShortL: case CharL: case LongL: case FloatL: case DoubleL:
      case VoidL: case StructL: case UnionL: case EnumL: case SignedL:
      case UnsignedL: case StaticL: case AutoL: case RegisterL: case ExternL:
         *Parser = PreState, CheckTrailingSemicolon = ParseDeclaration(Parser, Token);
      break;
      case DefineP: ParseMacroDefinition(Parser), CheckTrailingSemicolon = false; break;
#ifndef NO_HASH_INCLUDE
      case IncludeP:
         if (LexGetToken(Parser, &LexerValue, true) != StrLitL)
            ProgramFail(Parser, "\"filename.h\" expected");
         IncludeFile(Parser->pc, (char *)LexerValue->Val->Pointer);
         CheckTrailingSemicolon = false;
      break;
#endif
      case SwitchL: {
      // New block so we can store parser state (now made a block covering the whole case).
         if (LexGetToken(Parser, NULL, true) != LParL)
            ProgramFail(Parser, "'(' expected");
         int Label = ExpressionParseInt(Parser);
         if (LexGetToken(Parser, NULL, true) != RParL)
            ProgramFail(Parser, "')' expected");
         if (LexGetToken(Parser, NULL, false) != LCurlL)
            ProgramFail(Parser, "'{' expected");
         RunMode OldMode = Parser->Mode;
         int OldSearchLabel = Parser->SearchLabel;
         Parser->Mode = CaseM;
         Parser->SearchLabel = Label;
         ParseBlock(Parser, true, OldMode != SkipM && OldMode != ReturnM);
         if (Parser->Mode != ReturnM)
            Parser->Mode = OldMode;
         Parser->SearchLabel = OldSearchLabel;
         CheckTrailingSemicolon = false;
      }
      break;
      case CaseL: {
         bool DoRun = Parser->Mode == CaseM;
         if (DoRun) Parser->Mode = RunM;
         int Label = ExpressionParseInt(Parser);
         if (DoRun) Parser->Mode = CaseM;
         if (LexGetToken(Parser, NULL, true) != ColonL)
            ProgramFail(Parser, "':' expected");
         if (Parser->Mode == CaseM && Label == Parser->SearchLabel)
            Parser->Mode = RunM;
         CheckTrailingSemicolon = false;
      }
      break;
      case DefaultL:
         if (LexGetToken(Parser, NULL, true) != ColonL)
            ProgramFail(Parser, "':' expected");
         if (Parser->Mode == CaseM)
            Parser->Mode = RunM;
         CheckTrailingSemicolon = false;
      break;
      case BreakL:
         if (Parser->Mode == RunM)
            Parser->Mode = BreakM;
      break;
      case ContinueL:
         if (Parser->Mode == RunM)
            Parser->Mode = ContinueM;
      break;
      case ReturnL:
         if (Parser->Mode == RunM) {
            if (!Parser->pc->TopStackFrame || Parser->pc->TopStackFrame->ReturnValue->Typ->Base != VoidT) {
               Value CValue;
               if (!ExpressionParse(Parser, &CValue))
                  ProgramFail(Parser, "value required in return");
               if (!Parser->pc->TopStackFrame) // Return from top-level program?
                  PlatformExit(Parser->pc, ExpressionCoerceInteger(CValue));
               else
                  ExpressionAssign(Parser, Parser->pc->TopStackFrame->ReturnValue, CValue, true, NULL, 0, false);
               VariableStackPop(Parser, CValue);
            } else {
               Value CValue;
               if (ExpressionParse(Parser, &CValue))
                  ProgramFail(Parser, "value in return from a void function");
            }
            Parser->Mode = ReturnM;
         } else {
            Value CValue;
            ExpressionParse(Parser, &CValue);
         }
      break;
      case TypeDefL: ParseTypedef(Parser); break;
      case GotoL:
         if (LexGetToken(Parser, &LexerValue, true) != IdL)
            ProgramFail(Parser, "identifier expected");
         if (Parser->Mode == RunM) {
         // Start scanning for the goto label.
            Parser->SearchGotoLabel = LexerValue->Val->Identifier;
            Parser->Mode = GotoM;
         }
      break;
      case DeleteL: {
      // Try it as a function or variable name to delete.
         if (LexGetToken(Parser, &LexerValue, true) != IdL)
            ProgramFail(Parser, "identifier expected");
         if (Parser->Mode == RunM) {
         // Delete this variable or function.
            Value CValue = TableDelete(Parser->pc, &Parser->pc->GlobalTable, LexerValue->Val->Identifier);
            if (CValue == NULL)
               ProgramFail(Parser, "'%s' is not defined", LexerValue->Val->Identifier);
            VariableFree(Parser->pc, CValue);
         }
      }
      break;
      default: *Parser = PreState; return BadSyn;
   }
   if (CheckTrailingSemicolon) {
      if (LexGetToken(Parser, NULL, true) != SemiL)
         ProgramFail(Parser, "';' expected");
   }
   return OkSyn;
}

// Quick scan a source file for definitions.
void PicocParse(State pc, const char *FileName, const char *Source, int SourceLen, bool RunIt, bool CleanupNow, bool CleanupSource, bool EnableDebugger) {
   char *RegFileName = TableStrRegister(pc, FileName);
   void *Tokens = LexAnalyse(pc, RegFileName, Source, SourceLen, NULL);
// Allocate a cleanup node so we can clean up the tokens later.
   if (!CleanupNow) {
      CleanupTokenNode NewCleanupNode = HeapAllocMem(pc, sizeof *NewCleanupNode);
      if (NewCleanupNode == NULL)
         ProgramFailNoParser(pc, "out of memory");
      NewCleanupNode->Tokens = Tokens;
      NewCleanupNode->SourceText = CleanupSource? Source: NULL;
      NewCleanupNode->Next = pc->CleanupTokenList;
      pc->CleanupTokenList = NewCleanupNode;
   }
// Do the parsing.
   struct ParseState Parser;
   LexInitParser(&Parser, pc, Source, Tokens, RegFileName, RunIt, EnableDebugger);
   ParseResult Ok;
   do {
      Ok = ParseStatement(&Parser, true);
   } while (Ok == OkSyn);
   if (Ok == BadSyn)
      ProgramFail(&Parser, "parse error");
// Clean up.
   if (CleanupNow)
      HeapFreeMem(pc, Tokens);
}

// Parse interactively.
void PicocParseInteractiveNoStartPrompt(State pc, bool EnableDebugger) {
   struct ParseState Parser;
   LexInitParser(&Parser, pc, NULL, NULL, pc->StrEmpty, true, EnableDebugger);
   PicocPlatformSetExitPoint(pc);
   LexInteractiveClear(pc, &Parser);
   ParseResult Ok;
   do {
      LexInteractiveStatementPrompt(pc);
      Ok = ParseStatement(&Parser, true);
      LexInteractiveCompleted(pc, &Parser);
   } while (Ok == OkSyn);
   if (Ok == BadSyn)
      ProgramFail(&Parser, "parse error");
   PlatformPrintf(pc->CStdOut, "\n");
}

// Parse interactively, showing a startup message.
void PicocParseInteractive(State pc) {
   PlatformPrintf(pc->CStdOut, PromptStart);
   PicocParseInteractiveNoStartPrompt(pc, true);
}
