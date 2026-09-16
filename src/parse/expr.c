#include "internal.h"

#include <smew/parse.h>
#include <smew/ast.h>

#include <assert.h>
#include <ctype.h>

// XXX: I'm sure I missed some edge cases
static uint32_t cast_str_uint32(const char *src, Span span) {
	uint32_t num = 0;
	for (uint32_t i = 0; i < span.len; ++i) {
		unsigned char digit = (unsigned char)src[span.start + i];
		assert(isdigit(digit));
		num = num * 10 + (uint32_t)(src[span.start + i] - '0');
	}
	return num;
}

static AstLiteral *consume_literal_int(Parser *parser) {
	assert(parser->cur->kind == TOK_LITERAL_INT && "Unexpected token kind");
	AstLiteral *lit = parser_alloc_one(parser, AstLiteral);
	lit->kind = AST_LITERAL_INT;
	lit->span = parser->cur->span;
	lit->integer = cast_str_uint32(parser->src->data, lit->span);
	parser->cur++;
	return lit;
}

static int is_prefix_bp(uint8_t bp) {
	return bp != LOWEST_BP;
}

static int is_infix_bp(BindingPower bp) {
	return bp.left != LOWEST_BP || bp.right != LOWEST_BP;
}

static uint8_t get_expr_prefix_bp(TokenKind kind) {
	switch (kind) {
	case TOK_BANG: return 6;
	default:
		return LOWEST_BP;
	}
}

static BindingPower get_expr_infix_bp(TokenKind kind) {
	switch (kind) {
	case TOK_SEMICOLON: return (BindingPower){.left = 1, .right = 2        };
	case TOK_PLUS:      return (BindingPower){.left = 3, .right = 4        };
	case TOK_QUESTION:  return (BindingPower){.left = 5, .right = LOWEST_BP};
	default:
		return (BindingPower){.left = LOWEST_BP, .right = LOWEST_BP};
	}
}

static AstOpKindUnary cast_tok_to_prefix(TokenKind kind) {
	switch (kind) {
	case TOK_BANG:
		return AST_OP_UNARY_NOT;
	default:
		assert(0 && "Invalid prefix cast");
	}
}

static AstOpKindUnary cast_tok_to_postfix(TokenKind kind) {
	switch (kind) {
	case TOK_QUESTION:
		return AST_OP_UNARY_UNWRAP;
	default:
		assert(0 && "Invalid postfix cast");
	}
}

static AstOpKindBinary cast_tok_to_infix(TokenKind kind) {
	switch (kind) {
	case TOK_PLUS:      return AST_OP_BINARY_PLUS;
	case TOK_SEMICOLON: return AST_OP_BINARY_SEQ;
	default:
		assert(0 && "Invalid infix cast");
	}
}

// Synthesized unit (no source representation)
static AstExpr *unit_expr(Parser *parser) {
	AstExpr *expr = parser_alloc_one(parser, AstExpr);
	expr->span = zero_span();
	expr->kind = AST_EXPR_LITERAL;

	AstLiteral *lit = parser_alloc_one(parser, AstLiteral);
	lit->kind = AST_LITERAL_UNIT;

	expr->literal = lit;

	return expr;
}

static AstExpr *parse_expr_prefix(Parser *parser) {
	const Token *cur_tok = parser->cur;

	if (cur_tok->kind == TOK_LPAREN) {
		consume(parser, TOK_LPAREN);
		AstExpr *base_expr = parse_expr(parser, LOWEST_BP);
		consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");
		return base_expr;
	}

	if (cur_tok->kind == TOK_IDENTIFIER) {
		AstIdent *ident     = consume_ident(parser);
		AstExpr  *base_expr = parser_alloc_one(parser, AstExpr);

		base_expr->kind  = AST_EXPR_IDENT;
		base_expr->ident = ident;
		base_expr->span  = ident->span;

		return base_expr;
	}

	if (cur_tok->kind == TOK_LITERAL_INT) {
		AstLiteral *lit       = consume_literal_int(parser);
		AstExpr    *base_expr = parser_alloc_one(parser, AstExpr);

		base_expr->kind    = AST_EXPR_LITERAL;
		base_expr->literal = lit;
		base_expr->span    = lit->span;

		return base_expr;
	}

	uint8_t bp = get_expr_prefix_bp(cur_tok->kind);

	if (!is_prefix_bp(bp)) {
		return NULL;
	}

	const Token *op = cur_tok;
	consume(parser, op->kind);

	AstExpr *operand   = parse_expr(parser, bp);
	AstExpr *base_expr = parser_alloc_one(parser, AstExpr);

	base_expr->kind = AST_EXPR_OP_UNARY;

	AstOpUnary *unary = parser_alloc_one(parser, AstOpUnary);
	unary->span    = span_span(op->span, operand->span);
	unary->op      = cast_tok_to_prefix(op->kind);
	unary->operand = operand;

	base_expr->op_unary = unary;
	base_expr->span     = unary->span;

	return base_expr;
}

static AstExpr *parse_expr_infix(Parser *parser, AstExpr *base, uint8_t ambient_bp) {
	const Token *op = parser->cur;
	BindingPower bp = get_expr_infix_bp(op->kind);

	if (!is_infix_bp(bp))      return NULL;
	if (bp.left <= ambient_bp) return NULL;

	consume(parser, op->kind);

	if (bp.right == LOWEST_BP) {  // postfix
		AstExpr *new_base = parser_alloc_one(parser, AstExpr);
		new_base->kind    = AST_EXPR_OP_UNARY;

		AstOpUnary *unary = parser_alloc_one(parser, AstOpUnary);
		unary->span    = span_span(base->span, op->span);
		unary->op      = cast_tok_to_postfix(op->kind);
		unary->operand = base;

		new_base->op_unary = unary;
		new_base->span     = unary->span;

		return new_base;
	}

	// infix
	AstExpr *new_base = parser_alloc_one(parser, AstExpr);
	new_base->kind    = AST_EXPR_OP_BINARY;

	AstExpr     *operand = parse_expr(parser, bp.right);
	AstOpBinary *binary  = parser_alloc_one(parser, AstOpBinary);

	binary->span  = span_span(base->span, operand->span);
	binary->op    = cast_tok_to_infix(op->kind);
	binary->left  = base;
	binary->right = operand;

	new_base->op_binary = binary;
	new_base->span      = binary->span;

	return new_base;
}

AstExpr *parse_expr(Parser *parser, uint8_t ambient_bp) {
	AstExpr *base = parse_expr_prefix(parser);

	if (base == NULL) {
		return unit_expr(parser);
	}

	while (1) {
		AstExpr *new_base = parse_expr_infix(parser, base, ambient_bp);
		if (new_base == NULL) break;

		base = new_base;
	}
	return base;
}
