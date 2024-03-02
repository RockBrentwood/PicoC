// All platform-specific includes and defines go in this file.
#ifndef SYS_H
#define SYS_H

// Configurable options.
// Select your host type (or do it in the Makefile):
#if 0
#define UNIX_HOST
#define FLYINGFOX_HOST
#define SURVEYOR_HOST
#define SRV1_UNIX_HOST
#define UMON_HOST
#define WIN32 // (predefined on MSVC.)
#endif

#define LARGE_INT_POWER_OF_TEN 1000000000 // The largest power of ten which fits in an int on this architecture.
#if defined(__hppa__) || defined(__sparc__)
#   define ALIGN_TYPE double // The default data type to use for alignment.
#else
#   define ALIGN_TYPE void * // The default data type to use for alignment.
#endif
#define GLOBAL_TABLE_SIZE 97 // Global variable table.
#define STRING_TABLE_SIZE 97 // Shared string table size.
#define STRING_LITERAL_TABLE_SIZE 97 // String literal table size.
#define RESERVED_WORD_TABLE_SIZE 97 // Reserved word table size.
#define PARAMETER_MAX 16 // Maximum number of parameters to a function.
#define LINEBUFFER_MAX 256 // Maximum number of characters on a line.
#define LOCAL_TABLE_SIZE 11 // Size of local variable table (can expand).
#define STRUCT_TABLE_SIZE 11 // Size of struct/union member table (can expand).
#define INTERACTIVE_PROMPT_START "starting picoc " PICOC_VERSION "\n"
#define INTERACTIVE_PROMPT_STATEMENT "picoc> "
#define INTERACTIVE_PROMPT_LINE "     > "

// Host platform includes.
#ifdef UNIX_HOST
#   define USE_MALLOC_STACK // Stack is allocated using malloc().
#   define USE_MALLOC_HEAP // Heap is allocated using malloc().
#   include <stdio.h>
#   include <stdlib.h>
#   include <ctype.h>
#   include <string.h>
#   include <assert.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <unistd.h>
#   include <stdarg.h>
#   include <setjmp.h>
#   ifndef NO_FP
#      include <math.h>
#      define PICOC_MATH_LIBRARY
#      define USE_READLINE
#      undef BIG_ENDIAN
#      if defined(__powerpc__) || defined(__hppa__) || defined(__sparc__)
#         define BIG_ENDIAN
#      endif
#   endif
extern jmp_buf ExitBuf;
#else
#ifdef WIN32
#   define USE_MALLOC_STACK // Stack is allocated using malloc().
#   define USE_MALLOC_HEAP // Heap is allocated using malloc().
#   include <stdio.h>
#   include <stdlib.h>
#   include <ctype.h>
#   include <string.h>
#   include <assert.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <stdarg.h>
#   include <setjmp.h>
#   include <math.h>
#   define PICOC_MATH_LIBRARY
#   undef BIG_ENDIAN
extern jmp_buf ExitBuf;
#else
#   ifdef FLYINGFOX_HOST
#      define HEAP_SIZE (16*1024) // Space for the heap and the stack.
#      define NO_HASH_INCLUDE
#      include <stdlib.h>
#      include <ctype.h>
#      include <string.h>
#      include <sys/types.h>
#      include <stdarg.h>
#      include <setjmp.h>
#      include <math.h>
#      define assert(x)
#      define BUILTIN_MINI_STDLIB
#      undef BIG_ENDIAN
#   else
#   ifdef SURVEYOR_HOST
#      define HEAP_SIZE C_HEAPSIZE
#      define NO_FP
#      define NO_CTYPE
#      define NO_HASH_INCLUDE
#      define NO_MODULUS
#      include <cdefBF537.h>
#      include "../string.h"
#      include "../print.h"
#      include "../srv.h"
#      include "../setjmp.h"
#      include "../stdarg.h"
#      include "../colors.h"
#      include "../neural.h"
#      include "../gps.h"
#      include "../i2c.h"
#      include "../jpeg.h"
#      include "../malloc.h"
#      include "../xmodem.h"
#      define assert(x)
#      undef BIG_ENDIAN
#      define NO_CALLOC
#      define NO_REALLOC
#      define BROKEN_FLOAT_CASTS
#      define BUILTIN_MINI_STDLIB
#   else
#   ifdef UMON_HOST
#      define HEAP_SIZE (128*1024) // Space for the heap and the stack.
#      define NO_FP
#      define BUILTIN_MINI_STDLIB
#      include <stdlib.h>
#      include <string.h>
#      include <ctype.h>
#      include <sys/types.h>
#      include <stdarg.h>
#      include <math.h>
#      include "monlib.h"
#      define assert(x)
#      define malloc mon_malloc
#      define calloc(a, b) mon_malloc(a*b)
#      define realloc mon_realloc
#      define free mon_free
#   endif
#   endif
#   endif
extern int ExitBuf[];
#endif
#endif

#endif // SYS_H.
