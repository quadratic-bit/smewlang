#ifndef DIAG_H
#define DIAG_H

#include <smew/buf.h>
#include <smew/line.h>

typedef struct {
	Span span;
	const char *message;
	const char *expected;
} Diag;

typedef Vec(Diag) Diags;

void add_diag         (Diags *diags, Span span, const char *message);
void add_diag_expected(Diags *diags, Span span, const char *message, const char *expected);

// TODO:        v---- unify source file handling :3
void print_diag(const char *filename, const SourceBuffer *src, Diag *diag);

#endif
