lex: lex.c
	clang -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wundef -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wswitch-enum -Wimplicit-fallthrough -Wvla -g -fno-omit-frame-pointer -fsanitize=address,undefined vec.c lex.c buf.c -o ./lex
