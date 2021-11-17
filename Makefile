all: sl
clean:
	rm sl *.o
sl: sl.o
	clang $< -o $@
sl.o: main.c slip.binds
	clang -Werror -O0 -ggdb \
	-I. \
	-c $< -o $@
