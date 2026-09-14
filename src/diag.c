#include <smew/diag.h>

#include <smew/colors.h>

void print_diag(const char *filename, const SourceBuffer *src, Span span,
		const char *msg, const char *expect) {
	SourceLocation loc = locate_offset(src->data, span.start);
	printf("%s:%zu:%zu " CLR_RED "Error: %s." CLR_END,
		filename,
		loc.line + 1,
		loc.col  + 1,
		msg
	);
	if (expect[0] != '\0') {
		printf(" Expected: %s.", expect);
	}
	putchar('\n');
	size_t nl_cur = span.start;
	size_t left_pad = 0;
	while (nl_cur > 0 && src->data[nl_cur] != '\n') {
		nl_cur--;
	}
	if (src->data[nl_cur] == '\n') nl_cur++;
	for (size_t j = nl_cur; j < span.start; ++j) {
		putchar(src->data[j]);
		left_pad++;
	}
	printf("%.*s", (int)span.len, src->data + span.start);
	nl_cur = span.start + span.len;
	while (nl_cur < src->len && src->data[nl_cur] != '\n') {
		putchar(src->data[nl_cur]);
		nl_cur++;
	}
	putchar('\n');
	for (size_t j = 0; j < left_pad; j++) {
		putchar(' ');
	}
	printf(CLR_RED);
	putchar('^');
	for (size_t j = 1; j < span.len; ++j) {
		putchar('~');
	}
	puts(CLR_END);
}
