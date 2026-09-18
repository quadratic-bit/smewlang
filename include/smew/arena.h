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

/*
 * Region allocator for allocations with a shared lifetime.
 *
 * An arena owns all memory returned by `arena_alloc()`. Individual allocations
 * cannot be freed; `arena_free()` releases all allocations at once.
 *
 * A zeroed arena is valid. `arena_init()` must be called before `arena_alloc()`.
 *
 * Failed operations leave an arena unchanged.
 */
typedef struct {
	ArenaBlock *cur_block;
} Arena;

typedef enum {ARENA_OK, ARENA_ERR} ArenaResult;

/*
 * Initialize an empty arena.
 *
 * `arena` must be zeroed.
 *
 * Returns `ARENA_ERR` if the initial block cannot be allocated.
 */
ArenaResult arena_init (Arena *arena);

/*
 * Release every allocation owned by the arena and reset it to the empty state.
 *
 * Safe to call on an empty or previously freed arena.
 */
void arena_free(Arena *arena);

/*
 * Allocate `size` bytes with at least `align`-byte alignment.
 *
 * `arena` must be initialized. `size` must be nonzero. `align` must be nonzero and a power of two.
 *
 * Returns `NULL` if the request cannot be represented or memory allocation fails.
 *
 * Returned memory is uninitialized and remains valid until `arena_free()`.
 */
void *arena_alloc(Arena *arena, size_t size, size_t align);

#endif
