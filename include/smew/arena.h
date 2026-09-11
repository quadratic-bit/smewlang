#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct ArenaBlock ArenaBlock;
struct ArenaBlock {
	size_t      cap;
	size_t      len;
	ArenaBlock *prev;
	void       *data;
};

typedef struct {
	ArenaBlock *cur_block;
} Arena;

typedef enum {ARENA_OK, ARENA_ERR} ArenaResult;

ArenaResult arena_init (Arena *arena);
void        arena_free (Arena *arena);
void       *arena_alloc(Arena *arena, size_t size, size_t align);

#endif
