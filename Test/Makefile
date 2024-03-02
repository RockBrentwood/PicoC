RM=rm -f
APP=../picoc
TESTS=	00_assignment.T 01_comment.T 02_printf.T 03_struct.T 04_for.T \
	05_array.T 06_case.T 07_function.T 08_while.T 09_do_while.T \
	10_pointer.T 11_precedence.T 12_hashdefine.T 13_integer_literals.T 14_if.T \
	15_recursion.T 16_nesting.T 17_enum.T 18_include.T 19_pointer_arithmetic.T \
	20_pointer_comparison.T 21_char_array.T 22_floating_point.T 23_type_coercion.T 24_math_library.T \
	25_quicksort.T 26_character_constants.T 28_strings.T 29_array_address.T 30_hanoi.T \
	31_args.T 32_led.T 33_ternary_op.T 34_array_assignment.T 35_sizeof.T \
	36_array_initializers.T 37_sprintf.T 38_multiple_array_index.T 39_typedef.T 40_stdio.T \
	41_hashif.T 43_void_param.T 44_scoped_declarations.T 45_empty_for.T 47_switch_return.T \
	48_nested_break.T 49_bracket_evaluation.T 50_logical_second_arg.T 51_static.T 52_unnamed_enum.T \
	54_goto.T 55_array_initializer.T 56_cross_structure.T 57_macro_bug.T 58_return_outside.T \
	59_break_before_loop.T 60_local_vars.T 61_initializers.T 62_float.T 63_typedef.T \
	64_double_prefix_op.T 66_printf_undefined.T 67_macro_crash.T 68_return.T \

include CSmith/Makefile

%.T: %.X %.c
	@echo Test: $*...
	@if [ "x`echo $* | grep args`" != "x" ]; then \
		$(APP) $*.c - arg1 arg2 arg3 arg4 2>&1 >$*.Y; \
	else \
		$(APP) $*.c 2>&1 >$*.Y; \
	fi
	@if [ "x`diff -qbu $*.X $*.Y`" != "x" ]; then \
        	echo "error in test $*"; \
        	diff -u $*.X $*.Y; \
       		$(RM) $*.Y; \
		exit 1; \
	fi; \
       	$(RM) $*.Y

all: test
test: $(TESTS)
	@echo "test passed"
csmith: $(CSMITH_TESTS)
	@echo "CSmith test passed"
