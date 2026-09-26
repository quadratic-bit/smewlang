#include "type.h"

#include "expr.h"
#include "parse.h"

#include <smew/ast.h>
#include <smew/diag.h>
#include <smew/lex.h>
#include <smew/parse.h>
#include <smew/source.h>

#include <assert.h>
#include <stddef.h>


static BindingPower get_type_bp(TokenKind kind) {
	switch (kind) {
	case TOK_AMP:      return (BindingPower){.left = NO_BP, .right = 1    };
	case TOK_STAR:     return (BindingPower){.left = 2,     .right = NO_BP};
	case TOK_LBRACKET: return (BindingPower){.left = 3,     .right = NO_BP};
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
		AstType *base_type = parse_type(parser, MIN_BP);
		consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");
		return base_type;
	}

	if (cur_tok->kind == TOK_AMP) {
		uint8_t bp = get_type_bp(TOK_AMP).right;
		const Token *amp_tok = parser->cur;

		consume(parser, TOK_AMP);

		int mutable = consume_maybe(parser, TOK_KEY_MUT);

		AstType *operand   = parse_type(parser, bp);
		AstType *base_type = parser_alloc_one(parser, AstType);

		if (mutable) {
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

		base_type->kind       = AST_TYPE_NAME;
		base_type->name.ident = ident;
		base_type->span       = ident->span;
		return base_type;
	}

	return NULL;
}

static AstType *parse_type_postfix(Parser *parser, AstType *base, uint8_t min_bp) {
	const Token *cur_tok = parser->cur;

	if (cur_tok->kind == TOK_STAR) {
		uint8_t bp = get_type_bp(TOK_STAR).left;
		if (bp <= min_bp) return NULL;

		consume(parser, TOK_STAR);

		AstType *new_base = parser_alloc_one(parser, AstType);
		new_base->kind          = AST_TYPE_POINTER;
		new_base->pointer.inner = base;
		new_base->span          = span_span(base->span, cur_tok->span);
		return new_base;
	}

	if (cur_tok->kind == TOK_LBRACKET) {
		uint8_t bp = get_type_bp(TOK_LBRACKET).left;
		if (bp <= min_bp) return NULL;

		AstType *new_base = parser_alloc_one(parser, AstType);

		consume(parser, TOK_LBRACKET);
		if (parser->cur->kind == TOK_RBRACKET) {
			consume(parser, TOK_RBRACKET);

			new_base->kind            = AST_TYPE_ARRAY_DYN;
			new_base->array_dyn.inner = base;
			new_base->span            = span_span(base->span, prev(parser)->span);

			return new_base;
		}

		AstExpr *arr_len = parse_expr(parser, MIN_BP);
		consume_or_insert(parser, TOK_RBRACKET, "closing bracket");

		new_base->kind               = AST_TYPE_ARRAY_FIXED;
		new_base->array_fixed.inner  = base;
		new_base->array_fixed.length = arr_len;
		new_base->span               = span_span(base->span, prev(parser)->span);

		return new_base;
	}

	return NULL;
}

AstType *parse_type(Parser *parser, uint8_t min_bp) {
	AstType *base = parse_type_prefix(parser);

	if (base == NULL) {
		add_diag_expected(&parser->diags, parser->cur->span, "Unexpected token", "type");
		return unknown_type(parser);
	}

	if (parser->cur->kind == TOK_LPAREN) {
		consume(parser, TOK_LPAREN);

		AstTypeGeneric  *args = NULL;
		AstTypeGeneric **tail = &args;

		while (1) {
			AstType *arg = parse_type(parser, MIN_BP);

			AstTypeGeneric *generic = parser_alloc_one(parser, AstTypeGeneric);
			generic->arg  = arg;
			generic->next = NULL;

			*tail = generic;
			tail = &generic->next;

			if (parser->cur->kind == TOK_RPAREN) {
				break;
			}

			if (!consume_or_insert(parser, TOK_COMMA, "comma")) {
				break;
			}
		}

		const Token *rparen = parser->cur;
		consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");

		AstType *new_base = parser_alloc_one(parser, AstType);
		new_base->kind = AST_TYPE_GENERIC;
		new_base->generic.base = base;
		new_base->generic.args = args;
		new_base->span = span_span(base->span, rparen->span);

		base = new_base;
	}

	while (1) {
		AstType *new_base = parse_type_postfix(parser, base, min_bp);
		if (new_base == NULL) break;

		base = new_base;
	}
	return base;
}
