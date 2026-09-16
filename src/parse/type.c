#include "internal.h"

#include <smew/ast.h>
#include <smew/parse.h>

#include <assert.h>

static BindingPower get_type_bp(TokenKind kind) {
	switch (kind) {
	case TOK_AMP:      return (BindingPower){.left = LOWEST_BP, .right = 1        };
	case TOK_STAR:     return (BindingPower){.left = 2,         .right = LOWEST_BP};
	case TOK_LBRACKET: return (BindingPower){.left = 3,         .right = LOWEST_BP};
	default:
		assert(0 && "Unreachable");
	}
}

AstType *unknown_type(Parser *parser) {
	AstType *base_type = parser_alloc_one(parser, AstType);
	base_type->span = zero_span();
	base_type->kind = AST_TYPE_UNKNOWN;
	return base_type;
}

static AstType *parse_type_prefix(Parser *parser) {
	const Token *cur_tok = parser->cur;

	if (cur_tok->kind == TOK_LPAREN) {
		consume(parser, TOK_LPAREN);
		AstType *base_type = parse_type(parser, LOWEST_BP);
		consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");
		return base_type;
	}

	if (cur_tok->kind == TOK_AMP) {
		uint8_t bp = get_type_bp(TOK_AMP).right;
		const Token *amp_tok = parser->cur;

		consume(parser, TOK_AMP);

		cur_tok = parser->cur;

		AstType *operand   = parse_type(parser, bp);
		AstType *base_type = parser_alloc_one(parser, AstType);

		if (cur_tok->kind == TOK_KEY_MUT) {
			consume(parser, TOK_KEY_MUT);
			base_type->kind = AST_TYPE_BORROW_MUT;
			base_type->borrow_mut.inner = operand;
		} else {
			base_type->kind = AST_TYPE_BORROW;
			base_type->borrow.inner = operand;
		}

		base_type->span = span_span(amp_tok->span, operand->span);
		return base_type;
	}

	if (cur_tok->kind == TOK_IDENTIFIER) {
		AstIdent *ident     = consume_ident(parser);
		AstType  *base_type = parser_alloc_one(parser, AstType);

		base_type->kind = AST_TYPE_NAME;
		base_type->name.ident = ident;
		base_type->span = ident->span;
		return base_type;
	}

	return NULL;
}

static AstType *parse_type_postfix(Parser *parser, AstType *base, uint8_t ambient_bp) {
	const Token *cur_tok = parser->cur;

	if (cur_tok->kind == TOK_STAR) {
		uint8_t bp = get_type_bp(TOK_STAR).left;
		if (bp <= ambient_bp) return NULL;

		consume(parser, TOK_STAR);

		AstType *new_base = parser_alloc_one(parser, AstType);
		new_base->kind = AST_TYPE_POINTER;
		new_base->pointer.inner = base;
		new_base->span = span_span(base->span, cur_tok->span);
		return new_base;
	}

	if (cur_tok->kind == TOK_LBRACKET) {
		uint8_t bp = get_type_bp(TOK_LBRACKET).left;
		if (bp <= ambient_bp) return NULL;

		consume(parser, TOK_LBRACKET);
		consume(parser, TOK_RBRACKET); // TODO: array_fixed
		// TODO: recovery

		AstType *new_base = parser_alloc_one(parser, AstType);
		new_base->kind = AST_TYPE_ARRAY_DYN;
		new_base->array_dyn.inner = base;
		new_base->span = span_span(base->span, (cur_tok+1)->span);
		return new_base;
	}

	return NULL;
}

AstType *parse_type(Parser *parser, uint8_t ambient_bp) {
	AstType *base = parse_type_prefix(parser);

	if (base == NULL) {
		// XXX: it's not really a concrete token that's unexpected?
		// it's like we more expected some grammatic structure and found something else
		add_diag_expected(parser, AST_DIAG_UNEXPECTED_TOKEN, parser->cur->span, "type");
		return unknown_type(parser);
	}

	while (1) {
		AstType *new_base = parse_type_postfix(parser, base, ambient_bp);
		if (new_base == NULL) break;

		base = new_base;
	}
	return base;
}
