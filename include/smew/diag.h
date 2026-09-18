#ifndef DIAG_H
#define DIAG_H

#include <smew/source.h>

typedef struct {
	Span span;
	const char *message;
	const char *expected;
} Diag;

typedef Vec(Diag) Diags;

void add_diag         (Diags *diags, Span span, const char *message);
void add_diag_expected(Diags *diags, Span span, const char *message, const char *expected);

void print_diag(const SourceFile *src, Diag *diag);

#endif
