#include "vec.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

VecResult _vec_grow_impl(void **buf, size_t *cap, size_t elem_size, size_t new_size) {
	assert(*cap < new_size && "Cannot grow a buffer to a smaller size");

	if (new_size > SIZE_MAX / elem_size) {
		fputs("Cannot grow vector (requested new_size is too big).\n", stderr);
		return VEC_ERR;
	}

	void *new_data = realloc(*buf, new_size * elem_size);
	if (!new_data) {
		perror("vec_grow@realloc");
		return VEC_ERR;
	}

	*buf = new_data;
	*cap = new_size;
	return VEC_OK;
}


void _vec_init_impl(void **buf, size_t *cap, size_t *len, size_t elem_size, size_t init_cap) {
	*cap = init_cap;
	*len = 0;
	*buf = malloc(init_cap * elem_size);
	if (!*buf) {
		perror("buf_init@malloc");
		*cap = 0;
	}
}

VecResult _vec_push_impl(void **buf, size_t *cap, size_t *len, size_t elem_size, void *elem) {
	assert(*len <= *cap);

	if (*len == *cap) {
		size_t new_cap;

		if (*cap == 0) {
			new_cap = 1;
		} else {
			if (*cap > SIZE_MAX / 2) {
				fputs("Cannot push to vector (too big).\n", stderr);
				return VEC_ERR;
			}
			new_cap = *cap * 2;
		}

		if (_vec_grow_impl(buf, cap, elem_size, new_cap) == VEC_ERR) return VEC_ERR;
	}

	memcpy((char *)*buf + (*len * elem_size), elem, elem_size);
	(*len)++;

	return VEC_OK;
}
