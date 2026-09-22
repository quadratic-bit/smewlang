#include <smew/diag.h>

#include <smew/colors.h>
#include <smew/source.h>
#include <smew/vec.h>

#include <stdio.h>
#include <stdlib.h>

void add_diag(Diags *diags, Span span, const char *message) {
	Diag diag = (Diag){
		.span     = span,
		.message  = message,
		.expected = NULL
	};
	VecResult res = vec_push(diags, &diag);
	if (res != VEC_OK) {
		fprintf(stderr, "fatal: out of memory");
		exit(EXIT_FAILURE);
	}
}

void add_diag_expected(Diags *diags, Span span, const char *message, const char *expected) {
	Diag diag = (Diag){
		.span     = span,
		.message  = message,
		.expected = expected
	};
	VecResult res = vec_push(diags, &diag);
	if (res != VEC_OK) {
		fprintf(stderr, "fatal: out of memory");
		exit(EXIT_FAILURE);
	}
}

void print_diag(const SourceFile *src, const Diag *diag) {
	SourceLocation loc = locate_offset(src->buf.data, diag->span.start);
	printf("%s:%zu:%zu " CLR_RED "Error: %s." CLR_END,
		src->filename,
		loc.line + 1,
		loc.col  + 1,
		diag->message
	);
	if (diag->expected != NULL) {
		printf(" Expected: %s.", diag->expected);
	}
	putchar('\n');
	size_t nl_cur = diag->span.start;
	size_t left_pad = 0;
	while (nl_cur > 0 && src->buf.data[nl_cur] != '\n') {
		nl_cur--;
	}
	if (src->buf.data[nl_cur] == '\n') nl_cur++;
	for (size_t j = nl_cur; j < diag->span.start; ++j) {
		putchar(src->buf.data[j]);
		left_pad++;
	}
	printf("%.*s", (int)diag->span.len, src->buf.data + diag->span.start);
	nl_cur = diag->span.start + diag->span.len;
	while (nl_cur < src->buf.len && src->buf.data[nl_cur] != '\n') {
		putchar(src->buf.data[nl_cur++]);
	}
	putchar('\n');
	for (size_t j = 0; j < left_pad; j++) {
		putchar(' ');
	}
	printf(CLR_RED);
	putchar('^');
	for (size_t j = 1; j < diag->span.len; ++j) {
		putchar('~');
	}
	puts(CLR_END);
}
