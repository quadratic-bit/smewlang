#include "block.h"
#include "expr.h"
#include "parse.h"

#include <assert.h>

AstBlock *empty_block(Parser *parser) {
	AstBlock *block = parser_alloc_one(parser, AstBlock);
	block->span = zero_span();
	block->body = unit_expr(parser);
	return block;
}

AstBlock *parse_block(Parser *parser) {
	assert(parser->cur->kind == TOK_LBRACE && "Blocks must start with opening brace");

	const Token *block_start = parser->cur;
	AstBlock *block = parser_alloc_one(parser, AstBlock);
	block->body = NULL;

	consume(parser, TOK_LBRACE);

	AstExpr *expr = parse_expr(parser, MIN_BP);
	int changed = expr->span.len == 0;

	while (parser->cur->kind != TOK_RBRACE && parser->cur->kind != TOK_EOF) {
		// syntactic error -- recover by inserting a semicolon or advanvcing a cursor

		if (!changed) {
			add_diag_expected(&parser->diags, parser->cur->span,
					  "Unexpected token", "expression");
			parser->cur++;
		} else {
			add_diag_expected(&parser->diags, parser->cur->span,
					  "Unexpected token", "semicolon");
		}

		changed = parse_and_sequence(parser, &expr);
	}

	block->body = expr;

	if (!guard_eof(parser)) {
		consume(parser, TOK_RBRACE);
	}

	block->span = span_span(block_start->span, parser->cur->span);

	return block;
}
