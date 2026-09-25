#include "expr.h"

#include "block.h"
#include "parse.h"
#include "type.h"

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

static AstLiteral *consume_literal_str(Parser *parser) {
	assert(parser->cur->kind == TOK_LITERAL_STRING && "Unexpected token kind");

	AstLiteral *lit = parser_alloc_one(parser, AstLiteral);
	lit->kind = AST_LITERAL_STRING;
	lit->span = parser->cur->span;

	parser->cur++;

	return lit;
}

static AstLiteral *consume_literal_unit(Parser *parser) {
	assert(parser->cur->kind == TOK_KEY_UNIT && "Unexpected token kind");

	AstLiteral *lit = parser_alloc_one(parser, AstLiteral);
	lit->kind = AST_LITERAL_UNIT;
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
	case TOK_BANG:       return 14;
	case TOK_MINUS:      return 14;
	case TOK_STAR:       return 14;
	case TOK_AMP:        return 14;
	case TOK_KEY_MOVE:   return 3;
	case TOK_KEY_RETURN: return 3;
	default:
		return NO_BP;
	}
}

static BindingPower get_expr_infix_bp(TokenKind kind) {
	switch (kind) {
	case TOK_SEMICOLON: return (BindingPower){.left = 1,  .right = 2    };

	case TOK_ASSIGN:    return (BindingPower){.left = 5,  .right = 4    };

	case TOK_EQUAL:     return (BindingPower){.left = 6,  .right = 7    };
	case TOK_NOT_EQUAL: return (BindingPower){.left = 6,  .right = 7    };

	case TOK_GE:        return (BindingPower){.left = 8,  .right = 9    };
	case TOK_GT:        return (BindingPower){.left = 8,  .right = 9    };
	case TOK_LE:        return (BindingPower){.left = 8,  .right = 9    };
	case TOK_LT:        return (BindingPower){.left = 8,  .right = 9    };

	case TOK_PLUS:      return (BindingPower){.left = 10, .right = 11   };
	case TOK_MINUS:     return (BindingPower){.left = 10, .right = 11   };

	case TOK_SLASH:     return (BindingPower){.left = 12, .right = 13   };
	case TOK_STAR:      return (BindingPower){.left = 12, .right = 13   };

	case TOK_QUESTION:  return (BindingPower){.left = 15, .right = NO_BP};
	case TOK_LPAREN:    return (BindingPower){.left = 15, .right = 15   };
	case TOK_LBRACKET:  return (BindingPower){.left = 15, .right = 15   };

	case TOK_DOT:       return (BindingPower){.left = 16, .right = 17   };

	default:
		return (BindingPower){.left = NO_BP, .right = NO_BP};
	}
}

static AstOpKindUnary cast_tok_to_prefix(TokenKind kind) {
	switch (kind) {
	case TOK_BANG:       return AST_OP_UNARY_NOT;
	case TOK_MINUS:      return AST_OP_UNARY_MINUS;
	case TOK_STAR:       return AST_OP_UNARY_DEREF;
	case TOK_AMP:        return AST_OP_UNARY_BORROW;
	case TOK_KEY_MOVE:   return AST_OP_UNARY_MOVE;
	case TOK_KEY_RETURN: return AST_OP_UNARY_RETURN;
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

static AstBind *parse_bind(Parser *parser) {
	assert(parser->cur->kind == TOK_KEY_LET && "Unexpected token kind");
	const Token *let_tok = parser->cur;

	consume(parser, TOK_KEY_LET);

	AstBind *bind = parser_alloc_one(parser, AstBind);

	bind->mut = consume_maybe(parser, TOK_KEY_MUT);

	if (parser->cur->kind != TOK_IDENTIFIER) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token in bind expression", "identifier");
		bind->name = unknown_ident(parser);

		if (parser->cur->kind != TOK_COLON) {
			parser->cur++;  // maybe it's a keyword or a literal?
		}
	} else {
		bind->name = consume_ident(parser);
	}

	consume_or_insert(parser, TOK_COLON, "colon");

	bind->type = parse_type(parser, MIN_BP);

	consume_or_insert(parser, TOK_ASSIGN, "assignment");

	bind->value = parse_expr(parser, 4); // XXX: magic number + brittle + L + bozo

	if (bind->value == NULL) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token in bind expression", "expression");
		bind->value = unknown_expr(parser);
		bind->span  = span_span(let_tok->span, parser->cur->span);
	} else {
		bind->span  = span_span(let_tok->span, bind->type->span);
	}

	return bind;
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

static AstLoop *parse_loop(Parser *parser) {
	assert(parser->cur->kind == TOK_KEY_LOOP && "Unexpected token kind");

	const Token *loop_tok = parser->cur;
	consume(parser, TOK_KEY_LOOP);

	AstLoop *loop = parser_alloc_one(parser, AstLoop);

	if (parser->cur->kind != TOK_LBRACE) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token after `loop`", "opening brace");

		while (parser->cur->kind != TOK_LBRACE &&
		       parser->cur->kind != TOK_RBRACE &&
		       parser->cur->kind != TOK_EOF)
		{
			parser->cur++;
		}

		// XXX: code duplication from `parse_branch`
		if (parser->cur->kind != TOK_LBRACE) {
			loop->body = empty_block(parser);
			loop->span = span_span(loop_tok->span, parser->cur->span);
			return loop;
		}
	}

	loop->body = parse_block(parser);
	loop->span = span_span(loop_tok->span, loop->body->span);
	return loop;
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

	if (cur_tok->kind == TOK_LITERAL_STRING) {
		AstLiteral *lit       = consume_literal_str(parser);
		AstExpr    *base_expr = parser_alloc_one(parser, AstExpr);

		base_expr->kind    = AST_EXPR_LITERAL;
		base_expr->literal = lit;
		base_expr->span    = lit->span;

		return base_expr;
	}

	if (cur_tok->kind == TOK_KEY_UNIT) {
		AstLiteral *lit       = consume_literal_unit(parser);
		AstExpr    *base_expr = parser_alloc_one(parser, AstExpr);

		base_expr->kind    = AST_EXPR_LITERAL;
		base_expr->literal = lit;
		base_expr->span    = lit->span;

		return base_expr;
	}

	if (cur_tok->kind == TOK_KEY_LET) {
		AstBind *bind      = parse_bind(parser);
		AstExpr *base_expr = parser_alloc_one(parser, AstExpr);

		base_expr->kind = AST_EXPR_BIND;
		base_expr->bind = bind;
		base_expr->span = bind->span;

		return base_expr;
	}

	if (cur_tok->kind == TOK_KEY_WITH) {
		AstWith *with = parser_alloc_one(parser, AstWith);
		const Token *with_tok = parser->cur;
		consume(parser, TOK_KEY_WITH);

		AstBind  *bind  = parse_bind(parser);
		AstBlock *block = parse_block(parser);
		with->bind = bind;
		with->body = block;
		with->span = span_span(with_tok->span, block->span);

		AstExpr *base_expr = parser_alloc_one(parser, AstExpr);
		base_expr->kind = AST_EXPR_WITH;
		base_expr->with = with;
		base_expr->span = with->span;

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

	if (cur_tok->kind == TOK_KEY_LOOP) {
		AstLoop *loop      = parse_loop(parser);
		AstExpr *base_expr = parser_alloc_one(parser, AstExpr);

		base_expr->kind = AST_EXPR_LOOP;
		base_expr->loop = loop;
		base_expr->span = loop->span;

		return base_expr;
	}

	if (cur_tok->kind == TOK_KEY_BREAK) {
		AstBreak *brk       = parser_alloc_one(parser, AstBreak);
		AstExpr  *base_expr = parser_alloc_one(parser, AstExpr);

		brk->span       = cur_tok->span;
		base_expr->kind = AST_EXPR_BREAK;
		base_expr->brk  = brk;
		base_expr->span = brk->span;

		consume(parser, TOK_KEY_BREAK);

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

	if (operand == NULL) {
		operand = unit_expr(parser);
	}

	base_expr->kind = AST_EXPR_OP_UNARY;

	AstOpUnary *unary = parser_alloc_one(parser, AstOpUnary);
	unary->span    = span_span(op->span, operand->span);
	unary->op      = cast_tok_to_prefix(op->kind);
	unary->operand = operand;

	base_expr->op_unary = unary;
	base_expr->span     = unary->span;

	return base_expr;
}

static AstCallArg *parse_call_args(Parser *parser) {
	AstCallArg *arg = parser_alloc_one(parser, AstCallArg);
	arg->next = NULL;

	AstExpr *expr = parse_expr(parser, MIN_BP);

	if (expr == NULL) {
		add_diag_expected(&parser->diags, parser->cur->span,
				  "Unexpected token", "identifier");
		arg->arg = unknown_expr(parser);
	} else {
		arg->arg = expr;
	}

	if (parser->cur->kind == TOK_COMMA) {
		consume(parser, TOK_COMMA);
		arg->next = parse_call_args(parser);
	}

	return arg;
}

static AstLiteralStructField *parse_struct_lit_fields(Parser *parser) {
	AstLiteralStructField *field = parser_alloc_one(parser, AstLiteralStructField);
	field->next = NULL;

	Span span_start, span_end;

	if (parser->cur->kind == TOK_IDENTIFIER) {
		field->field = consume_ident(parser);
		span_start = field->field->span;
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");
		field->field = unknown_ident(parser);

		span_start = parser->cur->span;

		if (parser->cur->kind != TOK_COLON) {
			parser->cur++;  // maybe it's a keyword or a literal?
		}
	}

	consume_or_insert(parser, TOK_COLON, "colon");

	AstExpr *expr = parse_expr(parser, MIN_BP);

	if (expr == NULL) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");
		field->value = unknown_expr(parser);
		span_end = parser->cur->span;
	} else {
		field->value = expr;
		span_end = field->span;
	}

	if (parser->cur->kind == TOK_COMMA) {
		consume(parser, TOK_COMMA);
		field->next = parse_struct_lit_fields(parser);
	}

	field->span = span_span(span_start, span_end);

	return field;
}

static AstLiteral *parse_literal_struct(Parser *parser, AstIdent *name) {
	assert(parser->cur->kind == TOK_DOT && "Unexpected token kind");
	// XXX: next(parser)
	assert((parser->cur+1)->kind == TOK_LBRACE && "Unexpected token kind");

	AstLiteral *lit = parser_alloc_one(parser, AstLiteral);
	lit->kind = AST_LITERAL_STRUCT;

	consume(parser, TOK_DOT);
	consume(parser, TOK_LBRACE);

	AstStructLiteral *struc = parser_alloc_one(parser, AstStructLiteral);
	struc->type = name;

	if (parser->cur->kind == TOK_RBRACE) {
		consume(parser, TOK_RBRACE);
		struc->fields = NULL;
		lit->struc = struc;
		lit->span  = span_span(name->span, parser->cur->span);
		return lit;
	}

	struc->fields = parse_struct_lit_fields(parser);

	consume_or_insert(parser, TOK_RBRACE, "closing brace");

	lit->struc = struc;
	lit->span  = span_span(name->span, parser->cur->span);
	return lit;
}

static AstExpr *parse_expr_infix(Parser *parser, AstExpr *base, uint8_t min_bp) {
	const Token *op = parser->cur;

	if (base->kind == AST_EXPR_IDENT && op->kind == TOK_DOT &&
	    (parser->cur+1)->kind == TOK_LBRACE) { // XXX: next(parser)
		AstLiteral *lit       = parse_literal_struct(parser, base->ident);
		AstExpr    *base_expr = parser_alloc_one(parser, AstExpr);

		base_expr->kind    = AST_EXPR_LITERAL;
		base_expr->literal = lit;
		base_expr->span    = lit->span;

		return base_expr;
	}

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

	if (op->kind == TOK_LPAREN) {
		new_base->kind = AST_EXPR_CALL;

		AstCall    *call = parser_alloc_one(parser, AstCall);
		AstCallArg *args;

		if (parser->cur->kind != TOK_RPAREN) {
			args = parse_call_args(parser);
		} else {
			args = parser_alloc_one(parser, AstCallArg);
			args->arg  = NULL;
			args->next = NULL;
		}

		consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");

		call->callee = base;
		call->args   = args;
		call->span   = span_span(base->span, parser->cur->span);

		new_base->call = call;
		new_base->span = call->span;

		return new_base;
	}

	if (op->kind == TOK_LBRACKET) {
		new_base->kind = AST_EXPR_INDEX;

		AstExpr  *operand = parse_expr(parser, MIN_BP);
		AstIndex *index   = parser_alloc_one(parser, AstIndex);
		index->index = operand;

		if (operand == NULL) {
			operand = unit_expr(parser);
		}

		index->index = operand;
		index->base  = base;

		consume_or_insert(parser, TOK_RBRACKET, "closing bracket");

		index->span     = span_span(base->span, parser->cur->span);
		new_base->index = index;
		new_base->span  = index->span;
		return new_base;
	}

	new_base->kind = AST_EXPR_OP_BINARY;

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
