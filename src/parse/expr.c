#include "expr.h"

#include "block.h"
#include "parse.h"

#include <smew/ast.h>
#include <smew/diag.h>
#include <smew/lex.h>
#include <smew/parse.h>
#include <smew/source.h>

#include <assert.h>
#include <stddef.h>

static AstLiteral *consume_literal_int(Parser *parser) {
	assert(parser->cur->kind == TOK_LITERAL_INT && "Unexpected token kind");

	AstLiteral *lit = parser_alloc_one(parser, AstLiteral);
	lit->kind = AST_LITERAL_INT;
	lit->span = parser->cur->span;

	parser->cur++;

	return lit;
}

static int is_prefix_op(uint8_t bp) {
	return bp != NO_BP;
}

static int is_postfix_op(BindingPower bp) {
	return bp.left != NO_BP && bp.right == NO_BP;
}

static int is_infix_op(BindingPower bp) {
	return bp.left != NO_BP || bp.right != NO_BP;
}

static uint8_t get_expr_prefix_bp(TokenKind kind) {
	switch (kind) {
	case TOK_BANG: return 13;
	default:
		return NO_BP;
	}
}

static BindingPower get_expr_infix_bp(TokenKind kind) {
	switch (kind) {
	case TOK_SEMICOLON: return (BindingPower){.left = 1,  .right = 2    };

	case TOK_ASSIGN:    return (BindingPower){.left = 4,  .right = 3    };

	case TOK_EQUAL:     return (BindingPower){.left = 5,  .right = 6    };
	case TOK_NOT_EQUAL: return (BindingPower){.left = 5,  .right = 6    };

	case TOK_GE:        return (BindingPower){.left = 7,  .right = 8    };
	case TOK_GT:        return (BindingPower){.left = 7,  .right = 8    };
	case TOK_LE:        return (BindingPower){.left = 7,  .right = 8    };
	case TOK_LT:        return (BindingPower){.left = 7,  .right = 8    };

	case TOK_PLUS:      return (BindingPower){.left = 9,  .right = 10   };
	case TOK_MINUS:     return (BindingPower){.left = 9,  .right = 10   };

	case TOK_SLASH:     return (BindingPower){.left = 11, .right = 12   };
	case TOK_STAR:      return (BindingPower){.left = 11, .right = 12   };

	case TOK_QUESTION:  return (BindingPower){.left = 14, .right = NO_BP};

	case TOK_DOT:       return (BindingPower){.left = 15, .right = 16   };

	default:
		return (BindingPower){.left = NO_BP, .right = NO_BP};
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
	case TOK_SEMICOLON: return AST_OP_BINARY_SEQ;
	case TOK_ASSIGN:    return AST_OP_BINARY_ASSIGN;
	case TOK_EQUAL:     return AST_OP_BINARY_EQ;
	case TOK_NOT_EQUAL: return AST_OP_BINARY_NEQ;
	case TOK_GE:        return AST_OP_BINARY_GE;
	case TOK_GT:        return AST_OP_BINARY_GT;
	case TOK_LE:        return AST_OP_BINARY_LE;
	case TOK_LT:        return AST_OP_BINARY_LT;
	case TOK_PLUS:      return AST_OP_BINARY_PLUS;
	case TOK_MINUS:     return AST_OP_BINARY_MINUS;
	case TOK_SLASH:     return AST_OP_BINARY_DIV;
	case TOK_STAR:      return AST_OP_BINARY_MULT;
	case TOK_DOT:       return AST_OP_BINARY_ACCESSOR;
	default:
		assert(0 && "Invalid infix cast");
	}
}

AstExpr *unknown_expr(Parser *parser) {
	AstExpr *expr = parser_alloc_one(parser, AstExpr);
	expr->span = parser->cur->span;
	expr->kind = AST_EXPR_UNKNOWN;

	return expr;
}

// Synthesized unit (no source representation)
AstExpr *unit_expr(Parser *parser) {
	AstExpr *expr = parser_alloc_one(parser, AstExpr);
	expr->span = zero_span();
	expr->kind = AST_EXPR_LITERAL;

	AstLiteral *lit = parser_alloc_one(parser, AstLiteral);
	lit->kind = AST_LITERAL_UNIT;

	expr->literal = lit;

	return expr;
}

static AstCondBlock *parse_cond_block(Parser *parser, int is_else) {
	const Token *start = parser->cur;

	AstCondBlock *cond_block = parser_alloc_one(parser, AstCondBlock);
	cond_block->next = NULL;

	if (is_else) {
		cond_block->cond = unit_expr(parser);
	} else {
		cond_block->cond = parse_expr(parser, MIN_BP);

		if (cond_block->cond == NULL) {
			add_diag_expected(&parser->diags, parser->cur->span,
			                 "Unexpected token in branch condition", "expression");
			cond_block->cond = unknown_expr(parser);
		}
	}

	if (parser->cur->kind != TOK_LBRACE) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token after branch condition", "opening brace");

		while (parser->cur->kind != TOK_LBRACE &&
		       parser->cur->kind != TOK_RBRACE &&
		       parser->cur->kind != TOK_EOF)
		{
			parser->cur++;
		}

		if (parser->cur->kind != TOK_LBRACE) {
			cond_block->block = empty_block(parser);
			cond_block->span  = span_span(start->span, parser->cur->span);
			return cond_block;
		}
	}

	cond_block->block = parse_block(parser);
	cond_block->span  = span_span(start->span, cond_block->block->span);

	return cond_block;
}

static AstBranch *parse_branch(Parser *parser) {
	assert(parser->cur->kind == TOK_KEY_IF && "Unexpected token kind");

	const Token *if_tok = parser->cur;
	consume(parser, TOK_KEY_IF);

	AstBranch *br = parser_alloc_one(parser, AstBranch);
	br->conds = NULL;

	AstCondBlock **tail = &br->conds;

	AstCondBlock *cond_block = parse_cond_block(parser, 0);
	*tail = cond_block;
	tail = &cond_block->next;

	while (parser->cur->kind == TOK_KEY_ELSE) {
		consume(parser, TOK_KEY_ELSE);

		int is_else;

		if (parser->cur->kind == TOK_KEY_IF) {
			consume(parser, TOK_KEY_IF);
			is_else = 0;
		} else {
			is_else = 1;
		}

		cond_block = parse_cond_block(parser, is_else);

		*tail = cond_block;
		tail = &cond_block->next;
	}

	br->span = span_span(if_tok->span, cond_block->span);
	return br;
}

static AstExpr *parse_expr_prefix(Parser *parser) {
	const Token *cur_tok = parser->cur;

	if (cur_tok->kind == TOK_LPAREN) {
		consume(parser, TOK_LPAREN);
		AstExpr *base_expr = parse_expr(parser, MIN_BP);
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

	if (cur_tok->kind == TOK_KEY_IF) {
		AstBranch *br        = parse_branch(parser);
		AstExpr   *base_expr = parser_alloc_one(parser, AstExpr);

		base_expr->kind   = AST_EXPR_IF;
		base_expr->branch = br;
		base_expr->span   = br->span;

		return base_expr;
	}

	uint8_t bp = get_expr_prefix_bp(cur_tok->kind);

	if (!is_prefix_op(bp)) {
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

static AstExpr *parse_expr_infix(Parser *parser, AstExpr *base, uint8_t min_bp) {
	const Token *op = parser->cur;
	BindingPower bp = get_expr_infix_bp(op->kind);

	if (!is_infix_op(bp))  return NULL;
	if (bp.left <= min_bp) return NULL;

	consume(parser, op->kind);

	if (is_postfix_op(bp)) {
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

	AstExpr *new_base = parser_alloc_one(parser, AstExpr);
	new_base->kind    = AST_EXPR_OP_BINARY;

	AstExpr     *operand = parse_expr(parser, bp.right);
	AstOpBinary *binary  = parser_alloc_one(parser, AstOpBinary);

	AstOpKindBinary binary_op = cast_tok_to_infix(op->kind);

	if (operand == NULL) {
		if (binary_op == AST_OP_BINARY_SEQ) {
			binary->right = unit_expr(parser);
		} else {
			add_diag_expected(&parser->diags, parser->cur->span,
			                  "Unexpected token in binary expression", "rhs");
			binary->right = unknown_expr(parser);
		}
		binary->span  = span_span(base->span, op->span);
	} else {
		binary->span  = span_span(base->span, operand->span);
		binary->right = operand;
	}

	binary->op   = binary_op;
	binary->left = base;

	new_base->op_binary = binary;
	new_base->span      = binary->span;

	return new_base;
}

// Returns a valid AstExpr* on success, AST_EXPR_UNKNOWN expr on error, and NULL on abscence of expr
AstExpr *parse_expr(Parser *parser, uint8_t min_bp) {
	AstExpr *base = parse_expr_prefix(parser);

	if (base == NULL) {
		return NULL;
	}

	while (1) {
		AstExpr *new_base = parse_expr_infix(parser, base, min_bp);
		if (new_base == NULL) break;

		base = new_base;
	}
	return base;
}

int parse_and_sequence(Parser *parser, AstExpr **base) {
	AstExpr *right = parse_expr(parser, MIN_BP);
	if (right == NULL) return 0;

	AstOpBinary *seq = parser_alloc_one(parser, AstOpBinary);
	seq->op = AST_OP_BINARY_SEQ;

	if (*base == NULL) {
		*base = unit_expr(parser);
		seq->span = right->span;
	} else {
		seq->span = span_span((*base)->span, right->span);
	}

	seq->left  = *base;
	seq->right = right;

	AstExpr *new_base = parser_alloc_one(parser, AstExpr);
	new_base->kind    = AST_EXPR_OP_BINARY;

	new_base->op_binary = seq;
	new_base->span      = seq->span;

	*base = new_base;
	return 1;
}
