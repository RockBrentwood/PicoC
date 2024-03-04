RM=rm -f
CC=gcc
CFLAGS=-Wall -pedantic -g -DUNIX_HOST -DVER=\"2.1\"
LIBS=-lm -lreadline

APP	= PicoC
MOD	= \
	Main Table Lex Syn Exp Heap Type Var Lib Sys Inc Debug \
	Sys/SysUNIX Sys/LibUNIX \
	Lib/stdio Lib/math Lib/string Lib/stdlib Lib/time Lib/errno Lib/ctype Lib/stdbool Lib/unistd
SRC	:= $(MOD:%=%.c)
OBJ	:= $(MOD:%=%.o)

all: $(APP)
$(APP): $(OBJ)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@
csmith:	all
	(cd Test; make csmith)
test:	all
	(cd Test; make test)
clean:
	$(RM) $(OBJ)
	$(RM) *~
clobber: clean
	$(RM) $(APP)

count:
	@echo "Core:"
	@cat Main.h Extern.h Main.c Table.c Lex.c Syn.c Exp.c Sys.c Heap.c Type.c Var.c Inc.c Debug.c | grep -v '^[ 	]*/\*' | grep -v '^[ 	]*$$' | wc
	@echo ""
	@echo "Everything:"
	@cat $(SRC) *.h */*.h | wc

.PHONY: Lib.c

Main.o Syn.o Lib.o Sys.o Inc.o Sys/SysUNIX.o: Main.h
Table.o Lex.o Syn.o Exp.o Heap.o Type.o Var.o Lib.o Sys.o Inc.o Debug.o: Extern.h Sys.h
Sys/SysUNIX.o Sys/LibUNIX.o: Extern.h Sys.h
Lib/stdio.o Lib/math.o Lib/string.o Lib/stdlib.o Lib/time.o Lib/errno.o Lib/ctype.o Lib/stdbool.o Lib/unistd.o: Extern.h Sys.h
Main.o: Main.c
Table.o: Table.c
Lex.o: Lex.c
Syn.o: Syn.c
Exp.o: Exp.c
Heap.o: Heap.c
Type.o: Type.c
Var.o: Var.c
Lib.o: Lib.c
Sys.o: Sys.c
Inc.o: Inc.c
Debug.o: Debug.c
Sys/SysUNIX.o: Sys/SysUNIX.c
Sys/LibUNIX.o: Sys/LibUNIX.c
Lib/stdio.o: Lib/stdio.c
Lib/math.o: Lib/math.c
Lib/string.o: Lib/string.c
Lib/stdlib.o: Lib/stdlib.c
Lib/time.o: Lib/time.c
Lib/errno.o: Lib/errno.c
Lib/ctype.o: Lib/ctype.c
Lib/stdbool.o: Lib/stdbool.c
Lib/unistd.o: Lib/unistd.c
