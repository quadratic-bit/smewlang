#include <smew/source.h>
#include <smew/vec.h>

#include <stdint.h>

BufResult file_read(SourceFile *file, FILE *file_handler) {
	size_t n_read_bytes = 0;

	do {
		size_t n_reserved_bytes = file->buf.cap - file->buf.len;
		if (n_reserved_bytes == 0) {
			if (file->buf.cap > SIZE_MAX - file->buf.cap) {
				fputs("Input file size is too big.\n", stderr);
				return BUF_ERR;
			}

			size_t new_buf_size = file->buf.cap * 2;
			if (vec_grow(&file->buf, new_buf_size) != VEC_OK) return BUF_ERR;

			n_reserved_bytes = file->buf.cap - file->buf.len;
		}
		n_read_bytes = fread(file->buf.data + file->buf.len, 1,
		                     n_reserved_bytes, file_handler);
		file->buf.len += n_read_bytes;
	} while (n_read_bytes > 0);

	if (ferror(file_handler)) {
		perror("buf_read@fread");
		return BUF_ERR;
	}

	return BUF_OK;
}

// DREAM: cluster by graphemes for a better column counting
// PERF: can be optimized by prescanning line offsets and doing a binary search
SourceLocation locate_offset(const char *buffer, size_t offset) {
	size_t line = 0, line_offset = 0;
	for (size_t i = 1; i <= offset; ++i) {
		if (buffer[i - 1] == '\n') {
			line++;
			line_offset = i;
		}
	}
	if (buffer[0] == '\n') line++;

	return (SourceLocation){
		.line_offset = line_offset,
		.line        = line,
		.col         = offset - line_offset
	};
}

Span span_span(Span left, Span right) {
	return (Span){.start = left.start, .len = right.start + right.len - left.start};
}

Span zero_span(void) {
	return (Span){.start = 0, .len = 0};
}
