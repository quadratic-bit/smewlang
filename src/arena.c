#include <smew/arena.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: check usage on real workloads and maybe make dynamic
static const size_t ARENA_BLOCK_SIZE = 64 * 1024;

static ArenaResult arena_block_init(ArenaBlock **block, size_t requested) {
	ArenaBlock *new_block = malloc(sizeof *new_block);
	if (new_block == NULL) {
		perror("arena_block_init@malloc");
		return ARENA_ERR;
	}

	size_t alloc_size = requested > ARENA_BLOCK_SIZE
		? requested
		: ARENA_BLOCK_SIZE;

	void *data = malloc(alloc_size);
	if (data == NULL) {
		perror("arena_block_init@malloc");
		free(new_block);
		return ARENA_ERR;
	}

	new_block->cap  = alloc_size;
	new_block->len  = 0;
	new_block->prev = NULL;
	new_block->data = data;

	*block = new_block;
	return ARENA_OK;
}

ArenaResult arena_init(Arena *arena) {
	ArenaBlock *block;
	ArenaResult ret = arena_block_init(&block, 0);
	if (ret != ARENA_OK) {
		return ret;
	}

	arena->cur_block = block;
	return ARENA_OK;
}

void *arena_alloc(Arena *arena, size_t size, size_t align) {
	assert(align > 0 && "Alignment must not be zero");
	assert((align & (align - 1)) == 0 && "Alignment must not be a power of two");

	ArenaBlock *cur = arena->cur_block;

	uintptr_t addr;
retry:
	addr = (uintptr_t)cur->data + (uintptr_t)cur->len;

	if (addr > UINTPTR_MAX - ((uintptr_t)align - 1)) {
		return NULL;
	}

	uintptr_t aligned_addr = (addr + (uintptr_t)align - 1) & ~((uintptr_t)align - 1);

	size_t pad = (size_t)(aligned_addr - addr);

	if (pad > cur->cap - cur->len || size > cur->cap - cur->len - pad) {
		/* Worst case we'll need (align - 1) bytes of padding */
		if (size > SIZE_MAX - (align - 1)) return NULL;

		ArenaBlock *new_block;
		ArenaResult ret = arena_block_init(&new_block, size + align - 1);
		if (ret != ARENA_OK) return NULL;

		new_block->prev = cur;
		cur = arena->cur_block = new_block;

		goto retry;
	}

	cur->len += pad + size;

	return (void *)aligned_addr;
}

void arena_free(Arena *arena) {
	while (arena->cur_block != NULL) {
		free(arena->cur_block->data);
		ArenaBlock *cur = arena->cur_block;
		arena->cur_block = arena->cur_block->prev;
		free(cur);
	}
}
