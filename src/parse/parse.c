#include <smew/parse.h>

#include "expr.h"
#include "parse.h"
#include "type.h"

#include <smew/arena.h>
#include <smew/colors.h>
#include <smew/diag.h>
#include <smew/lex.h>
#include <smew/source.h>
#include <smew/vec.h>

#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void *arena_alloc_guarded(Arena *arena, size_t size, size_t align) {
	void *mem = arena_alloc(arena, size, align);
	if (mem == NULL) {
		fprintf(stderr, "fatal: out of memory");
		exit(EXIT_FAILURE);
	}
	return mem;
}

// TODO: tweak
static const size_t DEFAULT_AST_ITEMS_CAP = 8;

void consume(Parser *parser, TokenKind expect) {
	assert(parser->cur->kind == expect && "Unexpected token");
	parser->cur++;
}

static int consume_maybe(Parser *parser, TokenKind expected) {
	if (parser->cur->kind == expected) {
		parser->cur++;
		return 1;
	}
	return 0;
}

static int guard_eof(Parser *parser) {
	if (parser->cur->kind != TOK_EOF) return 0;
	add_diag(&parser->diags, parser->cur->span, "Unexpected EOF");
	return 1;
}

AstIdent *consume_ident(Parser *parser) {
	assert(parser->cur->kind == TOK_IDENTIFIER && "Unexpected token (expected identifier)");
	AstIdent *ident = parser_alloc_one(parser, AstIdent);
	ident->span = parser->cur->span;
	parser->cur++;
	return ident;
}

static AstIdent *unknown_ident(Parser *parser) {
	AstIdent *ident = parser_alloc_one(parser, AstIdent);
	ident->span = zero_span();
	return ident;
}

int consume_or_insert(Parser *parser, TokenKind expect, const char *expect_str) {
	if (parser->cur->kind != expect) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", expect_str);
		// assume it's inserted
		return 0;
	} else {
		consume(parser, expect);
		return 1;
	}
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
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token in struct field", "identifier");
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

	AstType *field_type = parse_type(parser, MIN_BP);
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
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token in struct definition", "identifier");
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
			add_diag_expected(&parser->diags, parser->cur->span,
			                  "Unexpected token in struct definition", "semicolon");
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
	param->type = parse_type(parser, MIN_BP);
	param->span = span_span(param->name->span, param->type->span);

	if (parser->cur->kind == TOK_COMMA) {
		consume(parser, TOK_COMMA);
		AstFunctionParam *next_param = consume_func_params(parser);
		param->next = next_param;
	}

	return param;
}

static AstBlock *consume_block(Parser *parser) {
	const Token *block_start = parser->cur;
	AstBlock *block = empty_block(parser);

	consume(parser, TOK_LBRACE);

	AstExpr *expr = parse_expr(parser, MIN_BP);

	while (parser->cur->kind != TOK_RBRACE && parser->cur->kind != TOK_EOF) {
		// syntactic error -- recover by inserting a semicolon

		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "semicolon");

		expr = parse_and_sequence(parser, expr);
	}

	block->body = expr;

	if (!guard_eof(parser)) {
		consume(parser, TOK_RBRACE);
	}

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

	func_def->return_type = parse_type(parser, MIN_BP);

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

Parser parse(const SourceFile *src, const Token *tokens) {
	Parser parser = (Parser){.src = src, .cur = tokens};
	arena_init(&parser.arena);
	vec_init  (&parser.tree.items, DEFAULT_AST_ITEMS_CAP);
	vec_init  (&parser.diags, 1);

	while (parser.cur->kind != TOK_EOF) {
		AstItem *item = parse_item(&parser);
		vec_push(&parser.tree.items, &item);
	}

	return parser;
}
