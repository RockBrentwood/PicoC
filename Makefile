RM=rm -f
CC=gcc
CFLAGS=-Wall -pedantic -g -DUNIX_HOST -DVER=\"2.1\"
LIBS=-lm -lreadline

APP	= picoc
MOD	= \
	picoc table lex parse expression heap type variable clibrary platform include debug \
	platform/platform_unix platform/library_unix \
	cstdlib/stdio cstdlib/math cstdlib/string cstdlib/stdlib cstdlib/time cstdlib/errno cstdlib/ctype cstdlib/stdbool cstdlib/unistd
SRC	:= $(MOD:%=%.c)
OBJ	:= $(MOD:%=%.o)

all: $(APP)
$(APP): $(OBJ)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@
csmith:	all
	(cd tests; make csmith)
test:	all
	(cd tests; make test)
clean:
	$(RM) $(OBJ)
	$(RM) *~
clobber: clean
	$(RM) $(APP)

count:
	@echo "Core:"
	@cat picoc.h interpreter.h picoc.c table.c lex.c parse.c expression.c platform.c heap.c type.c variable.c include.c debug.c | grep -v '^[ 	]*/\*' | grep -v '^[ 	]*$$' | wc
	@echo ""
	@echo "Everything:"
	@cat $(SRC) *.h */*.h | wc

.PHONY: clibrary.c

picoc.o parse.o clibrary.o platform.o include.o platform/platform_unix.o: picoc.h
table.o lex.o parse.o expression.o heap.o type.o variable.o clibrary.o platform.o include.o debug.o: interpreter.h platform.h
platform/platform_unix.o platform/library_unix.o: interpreter.h platform.h
cstdlib/stdio.o cstdlib/math.o cstdlib/string.o cstdlib/stdlib.o cstdlib/time.o cstdlib/errno.o cstdlib/ctype.o cstdlib/stdbool.o cstdlib/unistd.o: interpreter.h platform.h
picoc.o: picoc.c
table.o: table.c
lex.o: lex.c
parse.o: parse.c
expression.o: expression.c
heap.o: heap.c
type.o: type.c
variable.o: variable.c
clibrary.o: clibrary.c
platform.o: platform.c
include.o: include.c
debug.o: debug.c
platform/platform_unix.o: platform/platform_unix.c
platform/library_unix.o: platform/library_unix.c
cstdlib/stdio.o: cstdlib/stdio.c
cstdlib/math.o: cstdlib/math.c
cstdlib/string.o: cstdlib/string.c
cstdlib/stdlib.o: cstdlib/stdlib.c
cstdlib/time.o: cstdlib/time.c
cstdlib/errno.o: cstdlib/errno.c
cstdlib/ctype.o: cstdlib/ctype.c
cstdlib/stdbool.o: cstdlib/stdbool.c
cstdlib/unistd.o: cstdlib/unistd.c
