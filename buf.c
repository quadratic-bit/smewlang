#include <stdint.h>

#include "vec.h"
#include "buf.h"

BufResult buf_read(SourceBuffer *buf, FILE *input_file) {
	size_t n_read_bytes = 0;

	do {
		size_t n_reserved_bytes = buf->cap - buf->len;
		if (n_reserved_bytes == 0) {
			if (buf->cap > SIZE_MAX - buf->cap) {
				fputs("Input file size is too big.\n", stderr);
				return BUF_ERR;
			}

			size_t new_buf_size = buf->cap * 2;
			if (vec_grow(buf, new_buf_size) == VEC_ERR) return BUF_ERR;

			n_reserved_bytes = buf->cap - buf->len;
		}
		n_read_bytes = fread(buf->data + buf->len, 1, n_reserved_bytes, input_file);
		buf->len += n_read_bytes;
	} while (n_read_bytes > 0);

	if (ferror(input_file)) {
		perror("buf_read@fread");
		return BUF_ERR;
	}

	return BUF_OK;
}
