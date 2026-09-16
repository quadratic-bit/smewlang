CC := clang

CFLAGS := \
	-std=c17 \
	-Wall -Wextra -Wpedantic \
	-Wconversion -Wsign-conversion \
	-Wshadow -Wformat=2 -Wundef \
	-Wcast-qual -Wstrict-prototypes \
	-Wmissing-prototypes -Wswitch \
	-Wimplicit-fallthrough -Wvla \
	-g -fno-omit-frame-pointer \
	-fsanitize=address,undefined

CPPFLAGS := -Iinclude

SRC := $(wildcard src/*.c)

build/smewc: $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRC) -o $@
