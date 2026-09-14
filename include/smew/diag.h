#ifndef DIAG_H
#define DIAG_H

#include <smew/buf.h>
#include <smew/line.h>

#define Diag(Kind) \
	struct { \
		Kind kind; \
		Span span; \
	}

void print_diag(const char *filename, const SourceBuffer *src, const char *msg, Span span);

#endif
