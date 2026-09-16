#include <smew/parse.h>

#include <smew/arena.h>
#include <smew/colors.h>
#include <smew/lex.h>
#include <smew/line.h>
#include <smew/vec.h>

#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void *_arena_alloc_raw(Arena *arena, size_t size, size_t align) {
	void *mem = arena_alloc(arena, size, align);
	if (mem == NULL) {
		fprintf(stderr, "fatal: out of memory");
		exit(EXIT_FAILURE);
	}
	return mem;
}

#define parser_alloc_one(parser, type) \
	((type *)_arena_alloc_raw(&(parser)->arena, sizeof(type), alignof(type)))

// TODO: tweak
static const size_t DEFAULT_AST_ITEMS_CAP = 8;

static void consume(Parser *parser, TokenKind expected) {
	assert(parser->cur->kind == expected && "Unexpected token kind");
	parser->cur++;
}

static int consume_maybe(Parser *parser, TokenKind expected) {
	if (parser->cur->kind == expected) {
		parser->cur++;
		return 1;
	}
	return 0;
}

static void add_diag(Parser *parser, ParseDiagKind kind, Span span) {
	ParseDiag diag = {.kind = kind, .span = span, .expected = ""};
	VecResult res = vec_push(&parser->diags, &diag);
	if (res != VEC_OK) {
		fprintf(stderr, "fatal: out of memory");
		exit(EXIT_FAILURE);
	}
}

static void add_diag_expected(Parser *parser, ParseDiagKind kind, Span span, const char *expect) {
	ParseDiag diag = {.kind = kind, .span = span, .expected = expect};
	vec_push(&parser->diags, &diag);
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

static int guard_eof(Parser *parser) {
	if (parser->cur->kind != TOK_EOF) return 0;
	add_diag(parser, AST_DIAG_UNEXPECTED_EOF, parser->cur->span);
	return 1;
}

static AstIdent *consume_ident(Parser *parser) {
	assert(parser->cur->kind == TOK_IDENTIFIER && "Unexpected token kind");
	AstIdent *ident = parser_alloc_one(parser, AstIdent);
	ident->span = parser->cur->span;
	parser->cur++;
	return ident;
}

static Span zero_span(void) {
	return (Span){.start = 0, .len = 0};
}

static AstIdent *unknown_ident(Parser *parser) {
	AstIdent *ident = parser_alloc_one(parser, AstIdent);
	ident->span = zero_span();
	return ident;
}

typedef struct {
	uint8_t left;
	uint8_t right;
} BindingPower;

static const uint8_t LOWEST_BP = 0;

static BindingPower get_type_bp(TokenKind kind) {
	switch (kind) {
	case TOK_AMP:
		return (BindingPower){.left = 0, .right = 1};
	case TOK_STAR:
		return (BindingPower){.left = 2, .right = 0};
	case TOK_LBRACKET:
		return (BindingPower){.left = 3, .right = 0};
	default:
		assert(0 && "Unreachable");
	}
}

static Span span_span(Span left, Span right) {
	return (Span){.start = left.start, .len = right.start + right.len - left.start};
}

static int consume_or_insert(Parser *parser, TokenKind expect, const char *expect_str) {
	if (parser->cur->kind != expect) {
		add_diag_expected(parser, AST_DIAG_UNEXPECTED_TOKEN, parser->cur->span, expect_str);
		// assume it's inserted
		return 0;
	} else {
		consume(parser, expect);
		return 1;
	}
}

static AstType *unknown_type(Parser *parser) {
	AstType *base_type = parser_alloc_one(parser, AstType);
	base_type->span = zero_span();
	base_type->kind = AST_TYPE_UNKNOWN;
	return base_type;
}

static AstExpr *unknown_expr(Parser *parser) {
	AstExpr *base_expr = parser_alloc_one(parser, AstExpr);
	base_expr->span = zero_span();
	base_expr->kind = AST_EXPR_UNKNOWN;
	return base_expr;
}

static AstType *parse_type(Parser *parser, uint8_t ambient_bp);

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
		AstTypeKind borrow_kind = AST_TYPE_BORROW;
		if (cur_tok->kind == TOK_KEY_MUT) {
			borrow_kind = AST_TYPE_BORROW_MUT;
			consume(parser, TOK_KEY_MUT);
		}
		AstType *operand = parse_type(parser, bp);
		AstType *base_type = parser_alloc_one(parser, AstType);
		base_type->kind = borrow_kind;
		if (borrow_kind == AST_TYPE_BORROW) {
			base_type->borrow.inner = operand;
		} else if (borrow_kind == AST_TYPE_BORROW_MUT) {
			base_type->borrow_mut.inner = operand;
		} else assert(0);
		base_type->span = span_span(amp_tok->span, operand->span);
		return base_type;
	}
	if (cur_tok->kind == TOK_IDENTIFIER) {
		AstIdent *ident = consume_ident(parser);
		AstType *base_type = parser_alloc_one(parser, AstType);
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
		if (bp <= ambient_bp) {
			return NULL;
		}
		consume(parser, TOK_STAR);
		AstType *new_base = parser_alloc_one(parser, AstType);
		new_base->kind = AST_TYPE_POINTER;
		new_base->pointer.inner = base;
		new_base->span = span_span(base->span, cur_tok->span);
		return new_base;
	}
	if (cur_tok->kind == TOK_LBRACKET) {
		uint8_t bp = get_type_bp(TOK_LBRACKET).left;
		if (bp <= ambient_bp) {
			return NULL;
		}
		consume(parser, TOK_LBRACKET);
		consume(parser, TOK_RBRACKET); // TODO: array_fixed
		AstType *new_base = parser_alloc_one(parser, AstType);
		new_base->kind = AST_TYPE_ARRAY_DYN;
		new_base->array_dyn.inner = base;
		new_base->span = span_span(base->span, (cur_tok+1)->span);
		return new_base;
	}
	return NULL;
}

static AstType *parse_type(Parser *parser, uint8_t ambient_bp) {
	AstType *base = parse_type_prefix(parser);

	if (base == NULL) {
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

static void consume_until_semicolon_or_rbrace(Parser *parser) {
	while (parser->cur->kind != TOK_SEMICOLON &&
	       parser->cur->kind != TOK_RBRACE    &&
	       parser->cur->kind != TOK_EOF)
	{
		parser->cur++;
	}
}

static AstStructField *parse_struct_field(Parser *parser) {
	AstStructField *field = parser_alloc_one(parser, AstStructField);
	field->next = NULL;

	if (parser->cur->kind != TOK_IDENTIFIER) {
		add_diag_expected(parser, AST_DIAG_UNEXPECTED_TOKEN,
		                  parser->cur->span, "identifier");
		Span start = parser->cur->span;
		consume_until_semicolon_or_rbrace(parser);
		Span end = parser->cur->span;
		field->name = unknown_ident(parser);
		field->type = unknown_type(parser);
		field->span = span_span(start, end);
		return field;
	}

	AstIdent *field_name = consume_ident(parser);

	consume_or_insert(parser, TOK_COLON, "colon");

	AstType *field_type = parse_type(parser, LOWEST_BP);
	field->name = field_name;
	field->type = field_type;
	field->span = span_span(field_name->span, field_type->span);
	return field;
}

// XXX: brittle
static int is_item_start(TokenKind kind) {
	return kind == TOK_KEY_FN || kind == TOK_KEY_STRUCT || kind == TOK_KEY_PUB;
}

static AstStruct *parse_def_struct(Parser *parser) {
	const Token *struct_tok = parser->cur;
	consume(parser, TOK_KEY_STRUCT);

	AstStruct *struct_def = parser_alloc_one(parser, AstStruct);

	if (parser->cur->kind == TOK_IDENTIFIER) {
		struct_def->name = consume_ident(parser);
	} else {
		add_diag_expected(parser, AST_DIAG_UNEXPECTED_TOKEN,
		                  parser->cur->span, "identifier");
		while (parser->cur->kind != TOK_LBRACE && !is_item_start(parser->cur->kind)) {
			parser->cur++;
		}
		struct_def->name = unknown_ident(parser);
		if (parser->cur->kind != TOK_LBRACE) {
			struct_def->span = span_span(struct_tok->span, parser->cur->span);
			struct_def->fields = NULL;
			return struct_def;
		}
	}

	consume(parser, TOK_LBRACE);

	AstStructField **cur = &struct_def->fields;
	while (parser->cur->kind != TOK_RBRACE) {
		if (guard_eof(parser)) {
			struct_def->span = span_span(struct_tok->span, parser->cur->span);
			return struct_def;
		}
		AstStructField *field = parse_struct_field(parser);
		if (parser->cur->kind == TOK_SEMICOLON) {
			consume(parser, TOK_SEMICOLON);
		} else {
			add_diag_expected(parser, AST_DIAG_UNEXPECTED_TOKEN,
			                  parser->cur->span, "semicolon");
			consume_until_semicolon_or_rbrace(parser);
			consume_maybe(parser, TOK_SEMICOLON);
		}
		*cur = field;
		cur = &field->next;
	}
	struct_def->span = span_span(struct_tok->span, parser->cur->span);
	consume(parser, TOK_RBRACE);
	return struct_def;
}

// A synthetic one
static AstExpr *unit_expr(Parser *parser) {
	AstExpr    *expr    = parser_alloc_one(parser, AstExpr);
	AstLiteral *literal = parser_alloc_one(parser, AstLiteral);
	literal->span = zero_span();
	literal->kind = AST_LITERAL_UNIT;
	expr->span    = zero_span();
	expr->kind    = AST_EXPR_LITERAL;
	expr->literal = literal;
	return expr;
}

static AstBlock *empty_block(Parser *parser) {
	AstBlock *block = parser_alloc_one(parser, AstBlock);
	block->span = zero_span();
	block->body = NULL;
	return block;
}

static AstFunctionParam *consume_func_params(Parser *parser) {
	AstFunctionParam *param = parser_alloc_one(parser, AstFunctionParam);
	param->next = NULL;
	param->name = consume_ident(parser);
	consume(parser, TOK_COLON);
	param->type = parse_type(parser, LOWEST_BP);
	param->span = span_span(param->name->span, param->type->span);

	if (parser->cur->kind == TOK_COMMA) {
		consume(parser, TOK_COMMA);
		AstFunctionParam *next_param = consume_func_params(parser);
		param->next = next_param;
	}

	return param;
}

static uint8_t get_expr_prefix_bp(TokenKind kind) {
	switch (kind) {
	case TOK_BANG:
		return 4;
	default:
		return LOWEST_BP;
	}
}

static BindingPower get_expr_infix_bp(TokenKind kind) {
	switch (kind) {
	case TOK_PLUS:
		return (BindingPower){.left = 1, .right = 2};
	case TOK_BANG:
		return (BindingPower){.left = LOWEST_BP, .right = 4};
	case TOK_QUESTION:
		return (BindingPower){.left = 3, .right = LOWEST_BP};
	default:
		return (BindingPower){.left = LOWEST_BP, .right = LOWEST_BP};
	}
}

static AstExpr *parse_expr(Parser *parser, uint8_t ambient_bp);

static AstOpKindUnary cast_tok_to_prefix(TokenKind kind) {
	switch (kind) {
	case TOK_BANG:
		return AST_OP_UNARY_NOT;
	default:
		assert(0 && "Invalid tok -> prefix cast");
	}
}

static AstOpKindUnary cast_tok_to_postfix(TokenKind kind) {
	switch (kind) {
	case TOK_QUESTION:
		return AST_OP_UNARY_UNWRAP;
	default:
		assert(0 && "Invalid tok -> postfix cast");
	}
}

static AstOpKindBinary cast_tok_to_infix(TokenKind kind) {
	switch (kind) {
	case TOK_PLUS:
		return AST_OP_BINARY_PLUS;
	default:
		assert(0 && "Invalid tok -> postfix cast");
	}
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
		AstIdent *ident = consume_ident(parser);
		AstExpr *base_expr = parser_alloc_one(parser, AstExpr);
		base_expr->kind = AST_EXPR_IDENT;
		base_expr->ident = ident;
		base_expr->span = ident->span;
		return base_expr;
	}
	uint8_t bp = get_expr_prefix_bp(cur_tok->kind);

	if (bp == LOWEST_BP) {
		return NULL;
	}

	const Token *op = parser->cur;
	consume(parser, op->kind);
	AstExpr *operand = parse_expr(parser, bp);
	AstExpr *base_expr = parser_alloc_one(parser, AstExpr);
	base_expr->kind = AST_EXPR_OP_UNARY;

	AstOpUnary *unary = parser_alloc_one(parser, AstOpUnary);
	unary->span = span_span(op->span, operand->span);
	unary->op = cast_tok_to_prefix(op->kind);
	unary->operand = operand;
	base_expr->op_unary = unary;
	base_expr->span = unary->span; // TODO: remove span duplication
	return base_expr;
}

static AstExpr *parse_expr_postfix(Parser *parser, AstExpr *base, uint8_t ambient_bp) {
	const Token *op = parser->cur;
	if (op->kind == TOK_RBRACE || op->kind == TOK_EOF) return NULL;
	BindingPower bp = get_expr_infix_bp(op->kind);

	if (bp.left == LOWEST_BP) return NULL;
	if (bp.left <= ambient_bp) return NULL;

	consume(parser, op->kind);

	if (bp.right == LOWEST_BP) {
		// postfix
		AstExpr *new_base = parser_alloc_one(parser, AstExpr);
		new_base->kind = AST_EXPR_OP_UNARY;

		AstOpUnary *unary = parser_alloc_one(parser, AstOpUnary);
		unary->span = span_span(base->span, op->span);
		unary->op = cast_tok_to_postfix(op->kind);
		unary->operand = base;
		new_base->op_unary = unary;
		new_base->span = unary->span; // TODO: remove span duplication
		return new_base;
	}
	//infix
	AstExpr *new_base = parser_alloc_one(parser, AstExpr);
	new_base->kind = AST_EXPR_OP_BINARY;

	AstExpr *operand = parse_expr(parser, bp.right);
	AstOpBinary *binary = parser_alloc_one(parser, AstOpBinary);
	binary->span = span_span(base->span, operand->span);
	binary->op = cast_tok_to_infix(op->kind);
	binary->left = base;
	binary->right = operand;

	new_base->op_binary = binary;
	new_base->span = binary->span;
	return new_base;
}

static AstExpr *parse_expr(Parser *parser, uint8_t ambient_bp) {
	AstExpr *base = parse_expr_prefix(parser);

	if (base == NULL) {
		add_diag_expected(parser, AST_DIAG_UNEXPECTED_TOKEN,
		                  parser->cur->span, "expression");
		return unknown_expr(parser);
	}

	while (1) {
		AstExpr *new_base = parse_expr_postfix(parser, base, ambient_bp);
		if (new_base == NULL) break;

		base = new_base;
	}
	return base;
}

static AstSequence *consume_sequence(Parser *parser);

static AstExpr *consume_sequence_expr(Parser *parser) {
	AstSequence *seq  = consume_sequence(parser);
	AstExpr     *expr = unit_expr(parser);
	expr->kind = AST_EXPR_SEQUENCE;
	expr->span = seq->span;
	expr->seq  = seq;
	return expr;
}

static AstSequence *consume_sequence(Parser *parser) {
	AstSequence *seq = parser_alloc_one(parser, AstSequence);
	seq->left = parse_expr(parser, LOWEST_BP);
	if (parser->cur->kind == TOK_SEMICOLON) {
		consume(parser, TOK_SEMICOLON);
		seq->right = consume_sequence_expr(parser);
		seq->span = span_span(seq->left->span, seq->right->span);
		return seq;
	}
	seq->right = NULL;
	seq->span = seq->left->span;
	return seq;
}

static AstBlock *consume_block(Parser *parser) {
	const Token *block_start = parser->cur;
	AstBlock *block = empty_block(parser);

	consume(parser, TOK_LBRACE);

	AstExpr *seq = consume_sequence_expr(parser);
	block->body = seq;

	consume(parser, TOK_RBRACE);

	block->span = span_span(block_start->span, parser->cur->span);

	return block;
}

// TODO: recovery
static AstFunction *consume_def_func(Parser *parser) {
	assert((parser->cur->kind == TOK_KEY_PUB ||
	        parser->cur->kind == TOK_KEY_FN) && "Function must start with `fn` or `pub");

	const Token *start_tok = parser->cur;

	AstFunction *func_def = parser_alloc_one(parser, AstFunction);
	func_def->params = NULL;
	func_def->block  = empty_block(parser);

	func_def->is_public = consume_maybe(parser, TOK_KEY_PUB);
	consume_or_insert(parser, TOK_KEY_FN, "`fn` keyword");

	func_def->name = consume_ident(parser);

	consume(parser, TOK_LPAREN);
	if (parser->cur->kind != TOK_RPAREN) {
		func_def->params = consume_func_params(parser);
	}
	consume(parser, TOK_RPAREN);

	consume(parser, TOK_ARROW);

	func_def->return_type = parse_type(parser, LOWEST_BP);

	func_def->block = consume_block(parser);
	func_def->span = span_span(start_tok->span, func_def->block->span);

	return func_def;
}

static AstItem *parse_item(Parser *parser) {
	const Token *starting_token = parser->cur;

	AstItem *item = parser_alloc_one(parser, AstItem);

	switch (starting_token->kind) {
	case TOK_KEY_STRUCT:
		item->kind = AST_ITEM_STRUCT;
		item->struc = parse_def_struct(parser);
		item->span = item->struc->span;
		break;
	case TOK_KEY_PUB:
	case TOK_KEY_FN:
		item->kind = AST_ITEM_FUNCTION;
		item->function = consume_def_func(parser);
		item->span = item->function->span;
		break;
	default:
		assert(0 && "Unimplemented item");
	}
	return item;
}

Parser parse(const char *filename, Token *tokens) {
	Parser parser = (Parser){.filename = filename, .cur = tokens};
	arena_init(&parser.arena);
	vec_init  (&parser.tree.items, DEFAULT_AST_ITEMS_CAP);
	vec_init  (&parser.diags, 1);

	while (parser.cur->kind != TOK_EOF) {
		AstItem *item = parse_item(&parser);
		vec_push(&parser.tree.items, &item);
	}

	return parser;
}
