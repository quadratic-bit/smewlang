#ifndef PARSE_DIAG_H
#define PARSE_DIAG_H

#include <smew/parse.h>

void add_diag         (Parser *parser, ParseDiagKind kind, Span span);
void add_diag_expected(Parser *parser, ParseDiagKind kind, Span span, const char *expect);

#endif
