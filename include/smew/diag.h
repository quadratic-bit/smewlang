#ifndef DIAG_H
#define DIAG_H

#include <smew/buf.h>
#include <smew/line.h>

#define Diag(Kind) \
	struct { \
		Kind kind; \
		const char *expected; \
		Span span; \
	}

void print_diag(const char *filename, const SourceBuffer *src, Span span,
		const char *msg, const char *expect);

#endif
