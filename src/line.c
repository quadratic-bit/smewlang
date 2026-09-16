#include <smew/line.h>

// DREAM: cluster by graphemes for a better column counting
// PERF: ergh, can be optimized by prescanning line offsets and doing a binary search
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
