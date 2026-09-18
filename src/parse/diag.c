#include "diag.h"

void add_diag_expected(Parser *parser, ParseDiagKind kind, Span span, const char *expect) {
	ParseDiag diag = {.kind = kind, .span = span, .expected = expect};
	VecResult res = vec_push(&parser->diags, &diag);
	if (res != VEC_OK) {
		fprintf(stderr, "fatal: out of memory");
		exit(EXIT_FAILURE);
	}
}

void add_diag(Parser *parser, ParseDiagKind kind, Span span) {
	add_diag_expected(parser, kind, span, "");
}

static const char *diag_message(ParseDiag *diag) {
	switch (diag->kind) {
	case AST_DIAG_UNEXPECTED_EOF:
		return "Unexpected EOF";
	case AST_DIAG_UNEXPECTED_TOKEN:
		return "Unexpected token";
	}
}

void print_ast_diag(Parser *parser, SourceBuffer *src, ParseDiag *diag) {
	print_diag(parser->filename, src, diag->span, diag_message(diag), diag->expected);
}
