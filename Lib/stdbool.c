// stdbool.h library for large systems:
// Small embedded systems use Lib.c instead.
#include "../Extern.h"

#ifndef BUILTIN_MINI_STDLIB

static int trueValue = 1;
static int falseValue = 0;

// Structure definitions.
const char StdboolDefs[] = "typedef int bool;";

// Creates various system-dependent definitions.
void StdboolSetupFunc(Picoc *pc) {
// Defines.
   VariableDefinePlatformVar(pc, NULL, "true", &pc->IntType, (union AnyValue *)&trueValue, FALSE);
   VariableDefinePlatformVar(pc, NULL, "false", &pc->IntType, (union AnyValue *)&falseValue, FALSE);
   VariableDefinePlatformVar(pc, NULL, "__bool_true_false_are_defined", &pc->IntType, (union AnyValue *)&trueValue, FALSE);
}

#endif // !BUILTIN_MINI_STDLIB.
