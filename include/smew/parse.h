#ifndef PARSER_H
#define PARSER_H

#include <smew/arena.h>
#include <smew/ast.h>
#include <smew/diag.h>
#include <smew/lex.h>
#include <smew/vec.h>

#include <stdint.h>

typedef struct {
	const SourceFile *src;
	const Token      *cur;

	Ast   tree;
	Diags diags;
	Arena arena;
} Parser;

Parser parse(const SourceFile *src, const Token *tokens);

#endif
