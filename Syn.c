// picoc parser
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
ParseResult ParseStatementMaybeRun(ParseState Parser, bool Condition, bool CheckTrailingSemicolon) {
   if (Parser->Mode != RunModeSkip && !Condition) {
      RunMode OldMode = Parser->Mode;
      int Result;
      Parser->Mode = RunModeSkip;
      Result = ParseStatement(Parser, CheckTrailingSemicolon);
      Parser->Mode = OldMode;
      return Result;
   } else
      return ParseStatement(Parser, CheckTrailingSemicolon);
}

// Count the number of parameters to a function or macro.
int ParseCountParams(ParseState Parser) {
   int ParamCount = 0;
   LexToken Token = LexGetToken(Parser, NULL, true);
   if (Token != TokenCloseBracket && Token != TokenEOF) {
   // Count the number of parameters.
      ParamCount++;
      while ((Token = LexGetToken(Parser, NULL, true)) != TokenCloseBracket && Token != TokenEOF) {
         if (Token == TokenComma)
            ParamCount++;
      }
   }
   return ParamCount;
}

// Parse a function definition and store it for later.
Value ParseFunctionDefinition(ParseState Parser, ValueType ReturnType, char *Identifier) {
   ValueType ParamType;
   char *ParamIdentifier;
   LexToken Token = TokenNone;
   struct ParseState ParamParser;
   Value FuncValue;
   Value OldFuncValue;
   struct ParseState FuncBody;
   int ParamCount = 0;
   State pc = Parser->pc;
   if (pc->TopStackFrame != NULL)
      ProgramFail(Parser, "nested function definitions are not allowed");
   LexGetToken(Parser, NULL, true); // Open bracket.
   ParserCopy(&ParamParser, Parser);
   ParamCount = ParseCountParams(Parser);
   if (ParamCount > PARAMETER_MAX)
      ProgramFail(Parser, "too many parameters (%d allowed)", PARAMETER_MAX);
   FuncValue = VariableAllocValueAndData(pc, Parser, sizeof(struct FuncDef) + sizeof(ValueType)*ParamCount + sizeof(const char *)*ParamCount, false, NULL, true);
   FuncValue->Typ = &pc->FunctionType;
   FuncValue->Val->FuncDef.ReturnType = ReturnType;
   FuncValue->Val->FuncDef.NumParams = ParamCount;
   FuncValue->Val->FuncDef.VarArgs = false;
   FuncValue->Val->FuncDef.ParamType = (ValueType *)((char *)FuncValue->Val + sizeof(struct FuncDef));
   FuncValue->Val->FuncDef.ParamName = (char **)((char *)FuncValue->Val->FuncDef.ParamType + sizeof(ValueType)*ParamCount);
   for (ParamCount = 0; ParamCount < FuncValue->Val->FuncDef.NumParams; ParamCount++) {
   // Harvest the parameters into the function definition.
      if (ParamCount == FuncValue->Val->FuncDef.NumParams - 1 && LexGetToken(&ParamParser, NULL, false) == TokenEllipsis) {
      // Ellipsis at end.
         FuncValue->Val->FuncDef.NumParams--;
         FuncValue->Val->FuncDef.VarArgs = true;
         break;
      } else {
      // Add a parameter.
         TypeParse(&ParamParser, &ParamType, &ParamIdentifier, NULL);
         if (ParamType->Base == TypeVoid) {
         // This isn't a real parameter at all - delete it.
            ParamCount--;
            FuncValue->Val->FuncDef.NumParams--;
         } else {
            FuncValue->Val->FuncDef.ParamType[ParamCount] = ParamType;
            FuncValue->Val->FuncDef.ParamName[ParamCount] = ParamIdentifier;
         }
      }
      Token = LexGetToken(&ParamParser, NULL, true);
      if (Token != TokenComma && ParamCount < FuncValue->Val->FuncDef.NumParams - 1)
         ProgramFail(&ParamParser, "comma expected");
   }
   if (FuncValue->Val->FuncDef.NumParams != 0 && Token != TokenCloseBracket && Token != TokenComma && Token != TokenEllipsis)
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
   if (Token == TokenSemicolon)
      LexGetToken(Parser, NULL, true); // It's a prototype, absorb the trailing semicolon.
   else {
   // It's a full function definition with a body.
      if (Token != TokenLeftBrace)
         ProgramFail(Parser, "bad function definition");
      ParserCopy(&FuncBody, Parser);
      if (ParseStatementMaybeRun(Parser, false, true) != ParseResultOk)
         ProgramFail(Parser, "function definition expected");
      FuncValue->Val->FuncDef.Body = FuncBody;
      FuncValue->Val->FuncDef.Body.Pos = LexCopyTokens(&FuncBody, Parser);
   // Is this function already in the global table?
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
int ParseArrayInitializer(ParseState Parser, Value NewVariable, bool DoAssignment) {
   int ArrayIndex = 0;
   LexToken Token;
   Value CValue;
// Count the number of elements in the array.
   if (DoAssignment && Parser->Mode == RunModeRun) {
      struct ParseState CountParser;
      int NumElements;
      ParserCopy(&CountParser, Parser);
      NumElements = ParseArrayInitializer(&CountParser, NewVariable, false);
      if (NewVariable->Typ->Base != TypeArray)
         AssignFail(Parser, "%t from array initializer", NewVariable->Typ, NULL, 0, 0, NULL, 0);
      if (NewVariable->Typ->ArraySize == 0) {
         NewVariable->Typ = TypeGetMatching(Parser->pc, Parser, NewVariable->Typ->FromType, NewVariable->Typ->Base, NumElements, NewVariable->Typ->Identifier, true);
         VariableRealloc(Parser, NewVariable, TypeSizeValue(NewVariable, false));
      }
#ifdef DEBUG_ARRAY_INITIALIZER
      PRINT_SOURCE_POS;
      printf("array size: %d \n", NewVariable->Typ->ArraySize);
#endif
   }
// Parse the array initializer.
   Token = LexGetToken(Parser, NULL, false);
   while (Token != TokenRightBrace) {
      if (LexGetToken(Parser, NULL, false) == TokenLeftBrace) {
      // This is a sub-array initializer.
         int SubArraySize = 0;
         Value SubArray = NewVariable;
         if (Parser->Mode == RunModeRun && DoAssignment) {
            SubArraySize = TypeSize(NewVariable->Typ->FromType, NewVariable->Typ->FromType->ArraySize, true);
            SubArray = VariableAllocValueFromExistingData(Parser, NewVariable->Typ->FromType, (AnyValue)(&NewVariable->Val->ArrayMem[0] + SubArraySize*ArrayIndex), true, NewVariable);
#ifdef DEBUG_ARRAY_INITIALIZER
            int FullArraySize = TypeSize(NewVariable->Typ, NewVariable->Typ->ArraySize, true);
            PRINT_SOURCE_POS;
            PRINT_TYPE(NewVariable->Typ)
               printf("[%d] subarray size: %d (full: %d,%d) \n", ArrayIndex, SubArraySize, FullArraySize, NewVariable->Typ->ArraySize);
#endif
            if (ArrayIndex >= NewVariable->Typ->ArraySize)
               ProgramFail(Parser, "too many array elements");
         }
         LexGetToken(Parser, NULL, true);
         ParseArrayInitializer(Parser, SubArray, DoAssignment);
      } else {
         Value ArrayElement = NULL;
         if (Parser->Mode == RunModeRun && DoAssignment) {
            ValueType ElementType = NewVariable->Typ;
            int TotalSize = 1;
            int ElementSize = 0;
         // int x[3][3] = {1,2,3,4} => handle it just like int x[9] = {1,2,3,4}.
            while (ElementType->Base == TypeArray) {
               TotalSize *= ElementType->ArraySize;
               ElementType = ElementType->FromType;
            // char x[10][10] = {"abc", "def"} => assign "abc" to x[0], "def" to x[1] etc.
               if (LexGetToken(Parser, NULL, false) == TokenStringConstant && ElementType->FromType->Base == TypeChar)
                  break;
            }
            ElementSize = TypeSize(ElementType, ElementType->ArraySize, true);
#ifdef DEBUG_ARRAY_INITIALIZER
            PRINT_SOURCE_POS;
            printf("[%d/%d] element size: %d (x%d) \n", ArrayIndex, TotalSize, ElementSize, ElementType->ArraySize);
#endif
            if (ArrayIndex >= TotalSize)
               ProgramFail(Parser, "too many array elements");
            ArrayElement = VariableAllocValueFromExistingData(Parser, ElementType, (AnyValue)(&NewVariable->Val->ArrayMem[0] + ElementSize*ArrayIndex), true, NewVariable);
         }
      // This is a normal expression initializer.
         if (!ExpressionParse(Parser, &CValue))
            ProgramFail(Parser, "expression expected");
         if (Parser->Mode == RunModeRun && DoAssignment) {
            ExpressionAssign(Parser, ArrayElement, CValue, false, NULL, 0, false);
            VariableStackPop(Parser, CValue);
            VariableStackPop(Parser, ArrayElement);
         }
      }
      ArrayIndex++;
      Token = LexGetToken(Parser, NULL, false);
      if (Token == TokenComma) {
         LexGetToken(Parser, NULL, true);
         Token = LexGetToken(Parser, NULL, false);
      } else if (Token != TokenRightBrace)
         ProgramFail(Parser, "comma expected");
   }
   if (Token == TokenRightBrace)
      LexGetToken(Parser, NULL, true);
   else
      ProgramFail(Parser, "'}' expected");
   return ArrayIndex;
}

// Assign an initial value to a variable.
void ParseDeclarationAssignment(ParseState Parser, Value NewVariable, bool DoAssignment) {
   Value CValue;
   if (LexGetToken(Parser, NULL, false) == TokenLeftBrace) {
   // This is an array initializer.
      LexGetToken(Parser, NULL, true);
      ParseArrayInitializer(Parser, NewVariable, DoAssignment);
   } else {
   // This is a normal expression initializer.
      if (!ExpressionParse(Parser, &CValue))
         ProgramFail(Parser, "expression expected");
      if (Parser->Mode == RunModeRun && DoAssignment) {
         ExpressionAssign(Parser, NewVariable, CValue, false, NULL, 0, false);
         VariableStackPop(Parser, CValue);
      }
   }
}

// Declare a variable or function.
bool ParseDeclaration(ParseState Parser, LexToken Token) {
   char *Identifier;
   ValueType BasicType;
   ValueType Typ;
   Value NewVariable = NULL;
   bool IsStatic = false;
   bool FirstVisit = false;
   State pc = Parser->pc;
   TypeParseFront(Parser, &BasicType, &IsStatic);
   do {
      TypeParseIdentPart(Parser, BasicType, &Typ, &Identifier);
      if ((Token != TokenVoidType && Token != TokenStructType && Token != TokenUnionType && Token != TokenEnumType) && Identifier == pc->StrEmpty)
         ProgramFail(Parser, "identifier expected");
      if (Identifier != pc->StrEmpty) {
      // Handle function definitions.
         if (LexGetToken(Parser, NULL, false) == TokenOpenBracket) {
            ParseFunctionDefinition(Parser, Typ, Identifier);
            return false;
         } else {
            if (Typ == &pc->VoidType && Identifier != pc->StrEmpty)
               ProgramFail(Parser, "can't define a void variable");
            if (Parser->Mode == RunModeRun || Parser->Mode == RunModeGoto)
               NewVariable = VariableDefineButIgnoreIdentical(Parser, Identifier, Typ, IsStatic, &FirstVisit);
            if (LexGetToken(Parser, NULL, false) == TokenAssign) {
            // We're assigning an initial value.
               LexGetToken(Parser, NULL, true);
               ParseDeclarationAssignment(Parser, NewVariable, !IsStatic || FirstVisit);
            }
         }
      }
      Token = LexGetToken(Parser, NULL, false);
      if (Token == TokenComma)
         LexGetToken(Parser, NULL, true);
   } while (Token == TokenComma);
   return true;
}

// Parse a #define macro definition and store it for later.
void ParseMacroDefinition(ParseState Parser) {
   Value MacroName;
   char *MacroNameStr;
   Value ParamName;
   Value MacroValue;
   if (LexGetToken(Parser, &MacroName, true) != TokenIdentifier)
      ProgramFail(Parser, "identifier expected");
   MacroNameStr = MacroName->Val->Identifier;
   if (LexRawPeekToken(Parser) == TokenOpenMacroBracket) {
   // It's a parameterized macro, read the parameters.
      LexToken Token = LexGetToken(Parser, NULL, true);
      struct ParseState ParamParser;
      int NumParams;
      int ParamCount = 0;
      ParserCopy(&ParamParser, Parser);
      NumParams = ParseCountParams(&ParamParser);
      MacroValue = VariableAllocValueAndData(Parser->pc, Parser, sizeof(struct MacroDef) + sizeof(const char *)*NumParams, false, NULL, true);
      MacroValue->Val->MacroDef.NumParams = NumParams;
      MacroValue->Val->MacroDef.ParamName = (char **)((char *)MacroValue->Val + sizeof(struct MacroDef));
      Token = LexGetToken(Parser, &ParamName, true);
      while (Token == TokenIdentifier) {
      // Store a parameter name.
         MacroValue->Val->MacroDef.ParamName[ParamCount++] = ParamName->Val->Identifier;
      // Get the trailing comma.
         Token = LexGetToken(Parser, NULL, true);
         if (Token == TokenComma)
            Token = LexGetToken(Parser, &ParamName, true);
         else if (Token != TokenCloseBracket)
            ProgramFail(Parser, "comma expected");
      }
      if (Token != TokenCloseBracket)
         ProgramFail(Parser, "close bracket expected");
   } else {
   // Allocate a simple unparameterized macro.
      MacroValue = VariableAllocValueAndData(Parser->pc, Parser, sizeof(struct MacroDef), false, NULL, true);
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
   memcpy((void *)To, (void *)From, sizeof(*To));
}

// Copy where we're at in the parsing.
void ParserCopyPos(ParseState To, ParseState From) {
   To->Pos = From->Pos;
   To->Line = From->Line;
   To->HashIfLevel = From->HashIfLevel;
   To->HashIfEvaluateToLevel = From->HashIfEvaluateToLevel;
   To->CharacterPos = From->CharacterPos;
}

// Parse a "for" statement.
void ParseFor(ParseState Parser) {
   bool Condition;
   struct ParseState PreConditional;
   struct ParseState PreIncrement;
   struct ParseState PreStatement;
   struct ParseState After;
   RunMode OldMode = Parser->Mode;
   int PrevScopeID = 0, ScopeID = VariableScopeBegin(Parser, &PrevScopeID);
   if (LexGetToken(Parser, NULL, true) != TokenOpenBracket)
      ProgramFail(Parser, "'(' expected");
   if (ParseStatement(Parser, true) != ParseResultOk)
      ProgramFail(Parser, "statement expected");
   ParserCopyPos(&PreConditional, Parser);
   if (LexGetToken(Parser, NULL, false) == TokenSemicolon)
      Condition = true;
   else
      Condition = ExpressionParseInt(Parser) != 0;
   if (LexGetToken(Parser, NULL, true) != TokenSemicolon)
      ProgramFail(Parser, "';' expected");
   ParserCopyPos(&PreIncrement, Parser);
   ParseStatementMaybeRun(Parser, false, false);
   if (LexGetToken(Parser, NULL, true) != TokenCloseBracket)
      ProgramFail(Parser, "')' expected");
   ParserCopyPos(&PreStatement, Parser);
   if (ParseStatementMaybeRun(Parser, Condition, true) != ParseResultOk)
      ProgramFail(Parser, "statement expected");
   if (Parser->Mode == RunModeContinue && OldMode == RunModeRun)
      Parser->Mode = RunModeRun;
   ParserCopyPos(&After, Parser);
   while (Condition && Parser->Mode == RunModeRun) {
      ParserCopyPos(Parser, &PreIncrement);
      ParseStatement(Parser, false);
      ParserCopyPos(Parser, &PreConditional);
      if (LexGetToken(Parser, NULL, false) == TokenSemicolon)
         Condition = true;
      else
         Condition = ExpressionParseInt(Parser) != 0;
      if (Condition) {
         ParserCopyPos(Parser, &PreStatement);
         ParseStatement(Parser, true);
         if (Parser->Mode == RunModeContinue)
            Parser->Mode = RunModeRun;
      }
   }
   if (Parser->Mode == RunModeBreak && OldMode == RunModeRun)
      Parser->Mode = RunModeRun;
   VariableScopeEnd(Parser, ScopeID, PrevScopeID);
   ParserCopyPos(Parser, &After);
}

// Parse a block of code and return what mode it returned in.
RunMode ParseBlock(ParseState Parser, bool AbsorbOpenBrace, bool Condition) {
   int PrevScopeID = 0, ScopeID = VariableScopeBegin(Parser, &PrevScopeID);
   if (AbsorbOpenBrace && LexGetToken(Parser, NULL, true) != TokenLeftBrace)
      ProgramFail(Parser, "'{' expected");
   if (Parser->Mode == RunModeSkip || !Condition) {
   // Condition failed - skip this block instead.
      RunMode OldMode = Parser->Mode;
      Parser->Mode = RunModeSkip;
      while (ParseStatement(Parser, true) == ParseResultOk) {
      }
      Parser->Mode = OldMode;
   } else {
   // Just run it in its current mode.
      while (ParseStatement(Parser, true) == ParseResultOk) {
      }
   }
   if (LexGetToken(Parser, NULL, true) != TokenRightBrace)
      ProgramFail(Parser, "'}' expected");
   VariableScopeEnd(Parser, ScopeID, PrevScopeID);
   return Parser->Mode;
}

// Parse a typedef declaration.
void ParseTypedef(ParseState Parser) {
   ValueType Typ;
   ValueType *TypPtr;
   char *TypeName;
   struct Value InitValue;
   TypeParse(Parser, &Typ, &TypeName, NULL);
   if (Parser->Mode == RunModeRun) {
      TypPtr = &Typ;
      InitValue.Typ = &Parser->pc->TypeType;
      InitValue.Val = (AnyValue)TypPtr;
      VariableDefine(Parser->pc, Parser, TypeName, &InitValue, NULL, false);
   }
}

// Parse a statement.
ParseResult ParseStatement(ParseState Parser, bool CheckTrailingSemicolon) {
   Value CValue;
   Value LexerValue;
   Value VarValue;
   bool Condition;
   int Label;
   struct ParseState PreState;
   LexToken Token;
// If we're debugging, check for a breakpoint.
   if (Parser->DebugMode && Parser->Mode == RunModeRun)
      DebugCheckStatement(Parser);
// Take note of where we are and then grab a token to see what statement we have.
   ParserCopy(&PreState, Parser);
   Token = LexGetToken(Parser, &LexerValue, true);
   switch (Token) {
      case TokenEOF:
      return ParseResultEOF;
      case TokenIdentifier:
      // Might be a typedef-typed variable declaration or it might be an expression.
         if (VariableDefined(Parser->pc, LexerValue->Val->Identifier)) {
            VariableGet(Parser->pc, Parser, LexerValue->Val->Identifier, &VarValue);
            if (VarValue->Typ->Base == Type_Type) {
               *Parser = PreState;
               ParseDeclaration(Parser, Token);
               break;
            }
         } else {
         // It might be a goto label.
            LexToken NextToken = LexGetToken(Parser, NULL, false);
            if (NextToken == TokenColon) {
            // Declare the identifier as a goto label.
               LexGetToken(Parser, NULL, true);
               if (Parser->Mode == RunModeGoto && LexerValue->Val->Identifier == Parser->SearchGotoLabel)
                  Parser->Mode = RunModeRun;
               CheckTrailingSemicolon = false;
               break;
            }
#ifdef FEATURE_AUTO_DECLARE_VARIABLES
            else { // New_identifier = something.
         // Try to guess type and declare the variable based on assigned value.
               if (NextToken == TokenAssign && !VariableDefinedAndOutOfScope(Parser->pc, LexerValue->Val->Identifier)) {
                  if (Parser->Mode == RunModeRun) {
                     Value CValue;
                     char *Identifier = LexerValue->Val->Identifier;
                     LexGetToken(Parser, NULL, true);
                     if (!ExpressionParse(Parser, &CValue)) {
                        ProgramFail(Parser, "expected: expression");
                     }
#   if 0
                     PRINT_SOURCE_POS;
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
      // Else fallthrough to expression.
      // No break.
      case TokenAsterisk:
      case TokenAmpersand:
      case TokenIncrement:
      case TokenDecrement:
      case TokenOpenBracket:
         *Parser = PreState;
         ExpressionParse(Parser, &CValue);
         if (Parser->Mode == RunModeRun)
            VariableStackPop(Parser, CValue);
      break;
      case TokenLeftBrace:
         ParseBlock(Parser, false, true);
         CheckTrailingSemicolon = false;
      break;
      case TokenIf:
         if (LexGetToken(Parser, NULL, true) != TokenOpenBracket)
            ProgramFail(Parser, "'(' expected");
         Condition = ExpressionParseInt(Parser) != 0;
         if (LexGetToken(Parser, NULL, true) != TokenCloseBracket)
            ProgramFail(Parser, "')' expected");
         if (ParseStatementMaybeRun(Parser, Condition, true) != ParseResultOk)
            ProgramFail(Parser, "statement expected");
         if (LexGetToken(Parser, NULL, false) == TokenElse) {
            LexGetToken(Parser, NULL, true);
            if (ParseStatementMaybeRun(Parser, !Condition, true) != ParseResultOk)
               ProgramFail(Parser, "statement expected");
         }
         CheckTrailingSemicolon = false;
      break;
      case TokenWhile: {
         struct ParseState PreConditional;
         RunMode PreMode = Parser->Mode;
         if (LexGetToken(Parser, NULL, true) != TokenOpenBracket)
            ProgramFail(Parser, "'(' expected");
         ParserCopyPos(&PreConditional, Parser);
         do {
            ParserCopyPos(Parser, &PreConditional);
            Condition = ExpressionParseInt(Parser) != 0;
            if (LexGetToken(Parser, NULL, true) != TokenCloseBracket)
               ProgramFail(Parser, "')' expected");
            if (ParseStatementMaybeRun(Parser, Condition, true) != ParseResultOk)
               ProgramFail(Parser, "statement expected");
            if (Parser->Mode == RunModeContinue)
               Parser->Mode = PreMode;
         } while (Parser->Mode == RunModeRun && Condition);
         if (Parser->Mode == RunModeBreak)
            Parser->Mode = PreMode;
         CheckTrailingSemicolon = false;
      }
      break;
      case TokenDo: {
         struct ParseState PreStatement;
         RunMode PreMode = Parser->Mode;
         ParserCopyPos(&PreStatement, Parser);
         do {
            ParserCopyPos(Parser, &PreStatement);
            if (ParseStatement(Parser, true) != ParseResultOk)
               ProgramFail(Parser, "statement expected");
            if (Parser->Mode == RunModeContinue)
               Parser->Mode = PreMode;
            if (LexGetToken(Parser, NULL, true) != TokenWhile)
               ProgramFail(Parser, "'while' expected");
            if (LexGetToken(Parser, NULL, true) != TokenOpenBracket)
               ProgramFail(Parser, "'(' expected");
            Condition = ExpressionParseInt(Parser) != 0;
            if (LexGetToken(Parser, NULL, true) != TokenCloseBracket)
               ProgramFail(Parser, "')' expected");
         } while (Condition && Parser->Mode == RunModeRun);
         if (Parser->Mode == RunModeBreak)
            Parser->Mode = PreMode;
      }
      break;
      case TokenFor:
         ParseFor(Parser);
         CheckTrailingSemicolon = false;
      break;
      case TokenSemicolon:
         CheckTrailingSemicolon = false;
      break;
      case TokenIntType:
      case TokenShortType:
      case TokenCharType:
      case TokenLongType:
      case TokenFloatType:
      case TokenDoubleType:
      case TokenVoidType:
      case TokenStructType:
      case TokenUnionType:
      case TokenEnumType:
      case TokenSignedType:
      case TokenUnsignedType:
      case TokenStaticType:
      case TokenAutoType:
      case TokenRegisterType:
      case TokenExternType:
         *Parser = PreState;
         CheckTrailingSemicolon = ParseDeclaration(Parser, Token);
      break;
      case TokenHashDefine:
         ParseMacroDefinition(Parser);
         CheckTrailingSemicolon = false;
      break;
#ifndef NO_HASH_INCLUDE
      case TokenHashInclude:
         if (LexGetToken(Parser, &LexerValue, true) != TokenStringConstant)
            ProgramFail(Parser, "\"filename.h\" expected");
         IncludeFile(Parser->pc, (char *)LexerValue->Val->Pointer);
         CheckTrailingSemicolon = false;
      break;
#endif
      case TokenSwitch:
         if (LexGetToken(Parser, NULL, true) != TokenOpenBracket)
            ProgramFail(Parser, "'(' expected");
         Label = ExpressionParseInt(Parser);
         if (LexGetToken(Parser, NULL, true) != TokenCloseBracket)
            ProgramFail(Parser, "')' expected");
         if (LexGetToken(Parser, NULL, false) != TokenLeftBrace)
            ProgramFail(Parser, "'{' expected");
      {
      // New block so we can store parser state.
         RunMode OldMode = Parser->Mode;
         int OldSearchLabel = Parser->SearchLabel;
         Parser->Mode = RunModeCaseSearch;
         Parser->SearchLabel = Label;
         ParseBlock(Parser, true, (OldMode != RunModeSkip) && (OldMode != RunModeReturn));
         if (Parser->Mode != RunModeReturn)
            Parser->Mode = OldMode;
         Parser->SearchLabel = OldSearchLabel;
      }
         CheckTrailingSemicolon = false;
      break;
      case TokenCase:
         if (Parser->Mode == RunModeCaseSearch) {
            Parser->Mode = RunModeRun;
            Label = ExpressionParseInt(Parser);
            Parser->Mode = RunModeCaseSearch;
         } else
            Label = ExpressionParseInt(Parser);
         if (LexGetToken(Parser, NULL, true) != TokenColon)
            ProgramFail(Parser, "':' expected");
         if (Parser->Mode == RunModeCaseSearch && Label == Parser->SearchLabel)
            Parser->Mode = RunModeRun;
         CheckTrailingSemicolon = false;
      break;
      case TokenDefault:
         if (LexGetToken(Parser, NULL, true) != TokenColon)
            ProgramFail(Parser, "':' expected");
         if (Parser->Mode == RunModeCaseSearch)
            Parser->Mode = RunModeRun;
         CheckTrailingSemicolon = false;
      break;
      case TokenBreak:
         if (Parser->Mode == RunModeRun)
            Parser->Mode = RunModeBreak;
      break;
      case TokenContinue:
         if (Parser->Mode == RunModeRun)
            Parser->Mode = RunModeContinue;
      break;
      case TokenReturn:
         if (Parser->Mode == RunModeRun) {
            if (!Parser->pc->TopStackFrame || Parser->pc->TopStackFrame->ReturnValue->Typ->Base != TypeVoid) {
               if (!ExpressionParse(Parser, &CValue))
                  ProgramFail(Parser, "value required in return");
               if (!Parser->pc->TopStackFrame) // Return from top-level program?
                  PlatformExit(Parser->pc, ExpressionCoerceInteger(CValue));
               else
                  ExpressionAssign(Parser, Parser->pc->TopStackFrame->ReturnValue, CValue, true, NULL, 0, false);
               VariableStackPop(Parser, CValue);
            } else {
               if (ExpressionParse(Parser, &CValue))
                  ProgramFail(Parser, "value in return from a void function");
            }
            Parser->Mode = RunModeReturn;
         } else
            ExpressionParse(Parser, &CValue);
      break;
      case TokenTypedef:
         ParseTypedef(Parser);
      break;
      case TokenGoto:
         if (LexGetToken(Parser, &LexerValue, true) != TokenIdentifier)
            ProgramFail(Parser, "identifier expected");
         if (Parser->Mode == RunModeRun) {
         // Start scanning for the goto label.
            Parser->SearchGotoLabel = LexerValue->Val->Identifier;
            Parser->Mode = RunModeGoto;
         }
      break;
      case TokenDelete: {
      // Try it as a function or variable name to delete.
         if (LexGetToken(Parser, &LexerValue, true) != TokenIdentifier)
            ProgramFail(Parser, "identifier expected");
         if (Parser->Mode == RunModeRun) {
         // Delete this variable or function.
            CValue = TableDelete(Parser->pc, &Parser->pc->GlobalTable, LexerValue->Val->Identifier);
            if (CValue == NULL)
               ProgramFail(Parser, "'%s' is not defined", LexerValue->Val->Identifier);
            VariableFree(Parser->pc, CValue);
         }
      }
      break;
      default:
         *Parser = PreState;
      return ParseResultError;
   }
   if (CheckTrailingSemicolon) {
      if (LexGetToken(Parser, NULL, true) != TokenSemicolon)
         ProgramFail(Parser, "';' expected");
   }
   return ParseResultOk;
}

// Quick scan a source file for definitions.
void PicocParse(State pc, const char *FileName, const char *Source, int SourceLen, bool RunIt, bool CleanupNow, bool CleanupSource, bool EnableDebugger) {
   struct ParseState Parser;
   ParseResult Ok;
   CleanupTokenNode NewCleanupNode;
   char *RegFileName = TableStrRegister(pc, FileName);
   void *Tokens = LexAnalyse(pc, RegFileName, Source, SourceLen, NULL);
// Allocate a cleanup node so we can clean up the tokens later.
   if (!CleanupNow) {
      NewCleanupNode = HeapAllocMem(pc, sizeof(struct CleanupTokenNode));
      if (NewCleanupNode == NULL)
         ProgramFailNoParser(pc, "out of memory");
      NewCleanupNode->Tokens = Tokens;
      if (CleanupSource)
         NewCleanupNode->SourceText = Source;
      else
         NewCleanupNode->SourceText = NULL;
      NewCleanupNode->Next = pc->CleanupTokenList;
      pc->CleanupTokenList = NewCleanupNode;
   }
// Do the parsing.
   LexInitParser(&Parser, pc, Source, Tokens, RegFileName, RunIt, EnableDebugger);
   do {
      Ok = ParseStatement(&Parser, true);
   } while (Ok == ParseResultOk);
   if (Ok == ParseResultError)
      ProgramFail(&Parser, "parse error");
// Clean up.
   if (CleanupNow)
      HeapFreeMem(pc, Tokens);
}

// Parse interactively.
void PicocParseInteractiveNoStartPrompt(State pc, bool EnableDebugger) {
   struct ParseState Parser;
   ParseResult Ok;
   LexInitParser(&Parser, pc, NULL, NULL, pc->StrEmpty, true, EnableDebugger);
   PicocPlatformSetExitPoint(pc);
   LexInteractiveClear(pc, &Parser);
   do {
      LexInteractiveStatementPrompt(pc);
      Ok = ParseStatement(&Parser, true);
      LexInteractiveCompleted(pc, &Parser);
   } while (Ok == ParseResultOk);
   if (Ok == ParseResultError)
      ProgramFail(&Parser, "parse error");
   PlatformPrintf(pc->CStdOut, "\n");
}

// Parse interactively, showing a startup message.
void PicocParseInteractive(State pc) {
   PlatformPrintf(pc->CStdOut, INTERACTIVE_PROMPT_START);
   PicocParseInteractiveNoStartPrompt(pc, true);
}
