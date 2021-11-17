all: sl
clean:
	rm sl *.o

sl: sl.o
	clang $< -o $@
sl.o: main.c key.binds
	clang -Werror -O0 -ggdb \
	-I. -DSL_DUMP_OUT \
	-c $< -o $@
