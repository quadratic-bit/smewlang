#ifndef VEC_H
#define VEC_H

#include <stdio.h>
#include <stdlib.h>

typedef enum {VEC_OK, VEC_ERR} VecResult;

VecResult _vec_grow_impl(void **buf, size_t *cap,              size_t elem_size, size_t new_size);
void      _vec_init_impl(void **buf, size_t *cap, size_t *len, size_t elem_size, size_t init_cap);
VecResult _vec_push_impl(void **buf, size_t *cap, size_t *len, size_t elem_size, void  *elem    );

#define Vec(T)               \
	struct {             \
		T     *data; \
		size_t cap;  \
		size_t len;  \
	}

#define vec_free(vec)               \
	do {                        \
		free((vec)->data);  \
		(vec)->data = NULL; \
		(vec)->cap  = 0;    \
		(vec)->len  = 0;    \
	} while (0)


#define vec_grow(vec, new_size) \
	_vec_grow_impl((void **)&(vec)->data, &(vec)->cap, sizeof((vec)->data[0]), new_size)

#define vec_init(vec, size)                                                                     \
	_vec_init_impl((void **)&(vec)->data, &(vec)->cap, &(vec)->len, sizeof((vec)->data[0]), \
	               size)

#define vec_push(vec, elem)                                                                     \
	_vec_push_impl((void **)&(vec)->data, &(vec)->cap, &(vec)->len, sizeof((vec)->data[0]), \
	               (void *)(elem))

#endif
