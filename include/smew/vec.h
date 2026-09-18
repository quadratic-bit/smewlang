#ifndef VEC_H
#define VEC_H

#include <stdlib.h>

typedef enum {VEC_OK, VEC_ERR} VecResult;

VecResult vec_grow_impl(void **buf, size_t *cap,              size_t elem_size, size_t new_size);
VecResult vec_init_impl(void **buf, size_t *cap, size_t *len, size_t elem_size, size_t init_cap);
VecResult vec_push_impl(void **buf, size_t *cap, size_t *len, size_t elem_size, const void *elem);

/*
 * Declare a vector containing elements of type T.
 *
 * A vector shallowly owns `data` and must eventually be released with `vec_free()`.
 *
 * Invariants:
 *   len <= cap
 *   (cap == 0) => (data == NULL)
 *
 * Failed operations on a vector never invalidate an existing vector.
 *
 * A zeroed vector is valid and may be passed to `vec_push()` without calling `vec_init()`.
 */
#define Vec(T) \
	struct { \
		T     *data; \
		size_t cap; \
		size_t len; \
	}

/*
 * Release the vector's backing allocation and reset it to the empty state.
 *
 * Safe to call on an empty or freed vector.
 */
#define vec_free(vec) \
	do { \
		free((vec)->data); \
		(vec)->data = NULL; \
		(vec)->cap  = 0; \
		(vec)->len  = 0; \
	} while (0)

/*
 * Increase the vector capacity to exactly `new_size`.
 *
 * `new_size` must be greater than the current capacity.
 *
 * Returns `VEC_ERR` if the requested allocation is too large or allocation fails.
 */
#define vec_grow(vec, new_size) \
	vec_grow_impl((void **)&(vec)->data, &(vec)->cap, sizeof((vec)->data[0]), new_size)

/*
 * Initialize an empty vector with capacity `size`. `vec` must be zeroed before calling.
 *
 * `size` must be nonzero.
 *
 * Returns `VEC_ERR` if initialization fails.
 */
#define vec_init(vec, size) \
	vec_init_impl((void **)&(vec)->data, &(vec)->cap, &(vec)->len, sizeof((vec)->data[0]), \
	               size)

/*
 * Append a copy of `*elem` to the vector, growing it when necessary.
 *
 * `elem` must point to a valid object of the vector's element type.
 * `elem` must not point into the vector's `data` if this push can
 * cause the vector to grow, because `realloc()` may invalidate that pointer.
 *
 * Returns `VEC_ERR` if growth fails.
 */
#define vec_push(vec, elem) \
	vec_push_impl((void **)&(vec)->data, &(vec)->cap, &(vec)->len, sizeof((vec)->data[0]), \
	               (void *)(elem))

#endif
