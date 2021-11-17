all: sl

debug: sl
debug: def = -DSL_PARSE_ONLY -DSL_DUMP_TOK -DSL_DUMP_SYM
debug: flags = -O0 -ggdb

clean:
	rm -f sl *.o
sl: sl.o
	clang $< -o $@
sl.o: main.c slip.binds
	clang -Werror $(flags) \
	-I. $(def) \
	-c $< -o $@
