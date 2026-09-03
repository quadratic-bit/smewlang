#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char  *data;
	size_t cap;
	size_t len;
} SourceBuffer;

typedef enum {BUF_OK, BUF_ERR} BufResult;

static SourceBuffer buf_new(void) {
	SourceBuffer new_buf = {.cap = BUFSIZ, .len = 0};

	new_buf.data = malloc(new_buf.cap);
	if (!new_buf.data) {
		perror("buf_new@malloc");
		new_buf.cap = 0;
	}

	return new_buf;
}

static BufResult buf_grow(SourceBuffer *buf, size_t new_size) {
	assert(buf->cap < new_size && "Cannot grow a buffer to a smaller size");

	char *new_data = realloc(buf->data, new_size);
	if (!new_data) {
		perror("buf_grow@realloc");
		return BUF_ERR;
	}

	buf->data = new_data;
	buf->cap  = new_size;
	return BUF_OK;
}

static BufResult buf_read(SourceBuffer *buf, FILE *input_file) {
	size_t n_read_bytes = 0;

	do {
		size_t n_reserved_bytes = buf->cap - buf->len;
		if (n_reserved_bytes == 0) {
			if (buf->cap > SIZE_MAX - buf->cap) {
				fputs("Input file size is too big.\n", stderr);
				return BUF_ERR;
			}

			size_t new_buf_size = buf->cap * 2;
			if (buf_grow(buf, new_buf_size) == BUF_ERR) return BUF_ERR;

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

static void buf_free(SourceBuffer *buf) {
	free(buf->data);
	buf->data = NULL;
	buf->cap = buf->len = 0;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		puts("usage: lex <filename>");
		return 1;
	}

	const char *input_filename = argv[1];
	FILE *input_file = fopen(input_filename, "rb");
	if (!input_file) {
		perror("main@fopen");
		return 1;
	}

	SourceBuffer source = buf_new();
	if (!source.data) return 1;
	assert(source.cap > 0);
	assert(source.len == 0);

	if (buf_read(&source, input_file) == BUF_ERR) {
		buf_free(&source);
		fclose(input_file);
		return 1;
	}

	fwrite(source.data, 1, source.len, stdout);
	puts("\n");

	buf_free(&source);
	fclose(input_file);
	return 0;
}
