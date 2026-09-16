#ifndef PARSER_H
#define PARSER_H

#include <smew/arena.h>
#include <smew/ast.h>
#include <smew/diag.h>
#include <smew/lex.h>
#include <smew/vec.h>

#include <stdint.h>

typedef enum {
	AST_DIAG_UNEXPECTED_EOF,
	AST_DIAG_UNEXPECTED_TOKEN
} ParseDiagKind;

typedef Diag(ParseDiagKind) ParseDiag;

typedef Vec(ParseDiag) ParseDiags;

typedef struct {
	const char  *filename;
	const Token *cur;

	Ast tree;
	ParseDiags diags;

	Arena arena;
} Parser;

Parser parse(const char *filename, Token *tokens);

void print_ast_diag(Parser *parser, SourceBuffer *src, ParseDiag *diag);

#endif
