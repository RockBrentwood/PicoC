// ctype.h library for large systems:
// Small embedded systems use Lib.c instead.
#include <ctype.h>
#include "../Extern.h"

#ifndef BUILTIN_MINI_STDLIB

void StdIsalnum(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isalnum(Param[0]->Val->Integer);
}

void StdIsalpha(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isalpha(Param[0]->Val->Integer);
}

void StdIsblank(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   int ch = Param[0]->Val->Integer;
   ReturnValue->Val->Integer = ch == ' ' || ch == '\t';
}

void StdIscntrl(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = iscntrl(Param[0]->Val->Integer);
}

void StdIsdigit(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isdigit(Param[0]->Val->Integer);
}

void StdIsgraph(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isgraph(Param[0]->Val->Integer);
}

void StdIslower(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = islower(Param[0]->Val->Integer);
}

void StdIsprint(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isprint(Param[0]->Val->Integer);
}

void StdIspunct(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = ispunct(Param[0]->Val->Integer);
}

void StdIsspace(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isspace(Param[0]->Val->Integer);
}

void StdIsupper(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isupper(Param[0]->Val->Integer);
}

void StdIsxdigit(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isxdigit(Param[0]->Val->Integer);
}

void StdTolower(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = tolower(Param[0]->Val->Integer);
}

void StdToupper(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = toupper(Param[0]->Val->Integer);
}

void StdIsascii(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = isascii(Param[0]->Val->Integer);
}

void StdToascii(ParseState Parser, Value ReturnValue, Value *Param, int NumArgs) {
   ReturnValue->Val->Integer = toascii(Param[0]->Val->Integer);
}

// All string.h functions.
struct LibraryFunction StdCtypeFunctions[] = {
   { StdIsalnum, "int isalnum(int);" },
   { StdIsalpha, "int isalpha(int);" },
   { StdIsblank, "int isblank(int);" },
   { StdIscntrl, "int iscntrl(int);" },
   { StdIsdigit, "int isdigit(int);" },
   { StdIsgraph, "int isgraph(int);" },
   { StdIslower, "int islower(int);" },
   { StdIsprint, "int isprint(int);" },
   { StdIspunct, "int ispunct(int);" },
   { StdIsspace, "int isspace(int);" },
   { StdIsupper, "int isupper(int);" },
   { StdIsxdigit, "int isxdigit(int);" },
   { StdTolower, "int tolower(int);" },
   { StdToupper, "int toupper(int);" },
   { StdIsascii, "int isascii(int);" },
   { StdToascii, "int toascii(int);" },
   { NULL, NULL }
};

#endif // !BUILTIN_MINI_STDLIB.
