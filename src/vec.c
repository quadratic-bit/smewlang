#include <smew/vec.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

VecResult vec_grow_impl(void **buf, size_t *cap, size_t elem_size, size_t new_size) {
	assert(*cap < new_size && "Cannot grow a buffer to a smaller size");

	if (new_size > SIZE_MAX / elem_size) {
		return VEC_ERR;
	}

	void *new_data = realloc(*buf, new_size * elem_size);
	if (!new_data) {
		return VEC_ERR;
	}

	*buf = new_data;
	*cap = new_size;
	return VEC_OK;
}

VecResult vec_init_impl(void **buf, size_t *cap, size_t *len, size_t elem_size, size_t init_cap) {
	assert(init_cap > 0 && "Initial capacity must not be zero");
	assert(*buf == NULL && *cap == 0 && *len == 0 && "Vector must be in the empty state");

	if (init_cap > SIZE_MAX / elem_size) {
		return VEC_ERR;
	}

	void *data = malloc(init_cap * elem_size);
	if (data == NULL) {
		return VEC_ERR;
	}

	*buf = data;
	*cap = init_cap;
	*len = 0;

	return VEC_OK;
}

VecResult vec_push_impl(void **buf, size_t *cap, size_t *len, size_t elem_size, const void *elem) {
	assert(*len <= *cap);

	if (*len == *cap) {
		size_t new_cap;

		if (*cap == 0) {
			new_cap = 1;
		} else {
			if (*cap > SIZE_MAX / 2) {
				return VEC_ERR;
			}
			new_cap = *cap * 2;
		}

		VecResult grow_ret = vec_grow_impl(buf, cap, elem_size, new_cap);
		if (grow_ret != VEC_OK) return grow_ret;
	}

	memcpy((char *)*buf + (*len * elem_size), elem, elem_size);
	(*len)++;

	return VEC_OK;
}
