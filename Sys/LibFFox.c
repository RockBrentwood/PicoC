#include "../Extern.h"

// List of all library functions and their prototypes.
struct LibraryFunction PlatformLibrary[] = {
   { NULL, NULL }
};

void PlatformLibraryInit(State pc) {
   LibraryAdd(pc, &pc->GlobalTable, "platform library", &PlatformLibrary);
}
