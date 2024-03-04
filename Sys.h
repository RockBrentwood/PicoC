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

#if 0
// The largest power of ten which fits in an int on this architecture.
#   define Zillion 1000000000
#endif
// The default data type size to use for alignment.
#if defined __hppa__ || defined __sparc__
#define AlignSize sizeof(double)
#else
#define AlignSize sizeof(void *)
#endif
#define MemAlign(X) (((X) + AlignSize - 1)&~(AlignSize - 1))
#define AddAlign(X, N) ((char *)(X) + MemAlign(N))
#define SubAlign(X, N) ((char *)(X) - MemAlign(N))

#define GloTabMax 97		// The capacity for the global variable table.
#define StrTabMax 97		// The capacity for the shared string table.
#define LitTabMax 97		// The capacity for the string literal table.
#define KeyTabMax 97		// The capacity for the reserved word table.
#define ParameterMax 0x10	// The parameter count of the most egregious function allowed.
#define LineBufMax 0x100	// The character size of the longest line allowed.
#define LocTabMax 11		// The initial capacity of local variable (growable) tables.
#define MemTabMax 11		// The initial capacity of struct/union member (growable) tables.

#define PromptStart "starting picoc " PICOC_VERSION "\n"
#define PromptStatement "picoc> "
#define PromptLine "     > "

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
#      if defined __powerpc__ || defined __hppa__ || defined __sparc__
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
#      define HEAP_SIZE 0x4000 // Space for the heap and the stack.
#      define NO_HASH_INCLUDE
#      include <stdlib.h>
#      include <ctype.h>
#      include <string.h>
#      include <sys/types.h>
#      include <stdarg.h>
#      include <setjmp.h>
#      include <math.h>
#      define assert(X)
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
#      define assert(X)
#      undef BIG_ENDIAN
#      define NO_CALLOC
#      define NO_REALLOC
#      define BROKEN_FLOAT_CASTS
#      define BUILTIN_MINI_STDLIB
#   else
#   ifdef UMON_HOST
#      define HEAP_SIZE 0x20000 // Space for the heap and the stack.
#      define NO_FP
#      define BUILTIN_MINI_STDLIB
#      include <stdlib.h>
#      include <string.h>
#      include <ctype.h>
#      include <sys/types.h>
#      include <stdarg.h>
#      include <math.h>
#      include "monlib.h"
#      define assert(X)
#      define malloc mon_malloc
#      define calloc(A, B) mon_malloc(A*B)
#      define realloc mon_realloc
#      define free mon_free
#   endif
#   endif
#   endif
extern int ExitBuf[];
#endif
#endif

#endif // SYS_H.
