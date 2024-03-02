// stdbool.h library for large systems:
// Small embedded systems use Lib.c instead.
#include "../Extern.h"

#ifndef BUILTIN_MINI_STDLIB

static int trueValue = 1;
static int falseValue = 0;

// Structure definitions.
const char StdboolDefs[] = "typedef int bool;";

// Creates various system-dependent definitions.
void StdboolSetupFunc(State pc) {
// Defines.
   VariableDefinePlatformVar(pc, NULL, "true", &pc->IntType, (AnyValue)&trueValue, false);
   VariableDefinePlatformVar(pc, NULL, "false", &pc->IntType, (AnyValue)&falseValue, false);
   VariableDefinePlatformVar(pc, NULL, "__bool_true_false_are_defined", &pc->IntType, (AnyValue)&trueValue, false);
}

#endif // !BUILTIN_MINI_STDLIB.
