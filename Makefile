lex: ./src/lex.c ./src/buf.c ./src/vec.c
	clang -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wundef -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wswitch-enum -Wimplicit-fallthrough -Wvla -g -fno-omit-frame-pointer -fsanitize=address,undefined ./src/vec.c ./src/lex.c ./src/buf.c -I./include/ -o ./build/lex
