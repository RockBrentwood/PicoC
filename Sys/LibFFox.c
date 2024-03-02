#include "../Extern.h"

// List of all library functions and their prototypes.
struct LibraryFunction PlatformLibrary[] = {
   { NULL, NULL }
};

void PlatformLibraryInit() {
   LibraryAdd(&GlobalTable, "platform library", &PlatformLibrary);
}
