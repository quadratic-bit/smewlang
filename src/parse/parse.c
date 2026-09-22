#include <smew/parse.h>

#include "block.h"
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

const Token *prev(const Parser *parser) {
	return parser->cur - 1;
}

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

int guard_eof(Parser *parser) {
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
	return kind == TOK_KEY_FN     ||
	       kind == TOK_KEY_STRUCT ||
	       kind == TOK_KEY_PUB    ||
	       kind == TOK_EOF;
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
			struct_def->span   = span_span(struct_tok->span, parser->cur->span);
			struct_def->fields = NULL;
			return struct_def;
		}
	}

	if (parser->cur->kind != TOK_LBRACE) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token in struct definition", "opening brace");
		while (parser->cur->kind != TOK_LBRACE && !is_item_start(parser->cur->kind)) {
			parser->cur++;
		}
		if (parser->cur->kind != TOK_LBRACE) {
			struct_def->span   = span_span(struct_tok->span, parser->cur->span);
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

static AstFunctionParam *parse_func_params(Parser *parser) {
	AstFunctionParam *param = parser_alloc_one(parser, AstFunctionParam);
	param->next = NULL;

	if (parser->cur->kind == TOK_IDENTIFIER) {
		param->name = consume_ident(parser);
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");
		param->name = unknown_ident(parser);
	}

	consume_or_insert(parser, TOK_COLON, "colon");

	param->type = parse_type(parser, MIN_BP);
	param->span = span_span(param->name->span, param->type->span);

	if (parser->cur->kind == TOK_COMMA) {
		consume(parser, TOK_COMMA);
		AstFunctionParam *next_param = parse_func_params(parser);
		param->next = next_param;
	}

	return param;
}

static AstFunction *parse_def_func(Parser *parser) {
	assert((parser->cur->kind == TOK_KEY_PUB ||
	        parser->cur->kind == TOK_KEY_FN) && "Function must start with `fn` or `pub");

	const Token *start_tok = parser->cur;

	AstFunction *func_def = parser_alloc_one(parser, AstFunction);
	func_def->params = NULL;

	func_def->is_public = consume_maybe(parser, TOK_KEY_PUB);
	consume_or_insert(parser, TOK_KEY_FN, "`fn` keyword");

	if (parser->cur->kind == TOK_IDENTIFIER) {
		func_def->name = consume_ident(parser);
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");

		func_def->name = unknown_ident(parser);

		while (parser->cur->kind != TOK_LPAREN &&
		       parser->cur->kind != TOK_LBRACE &&
		       parser->cur->kind != TOK_EOF) {
			parser->cur++;
		}

		if (guard_eof(parser)) {
			func_def->span        = span_span(start_tok->span, prev(parser)->span);
			func_def->block       = empty_block (parser);
			func_def->return_type = unknown_type(parser);
			return func_def;
		}
	}

	if (parser->cur->kind == TOK_LPAREN) {
		consume(parser, TOK_LPAREN);

		if (parser->cur->kind != TOK_RPAREN) {
			func_def->params = parse_func_params(parser);
		}

		consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "opening parenthesis (function params)");
	}

	consume_or_insert(parser, TOK_ARROW, "arrow");

	func_def->return_type = parse_type(parser, MIN_BP);

	if (parser->cur->kind != TOK_LBRACE) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token after function declaration", "opening brace");
		while (parser->cur->kind != TOK_LBRACE && !is_item_start(parser->cur->kind)) {
			parser->cur++;
		}
		if (parser->cur->kind != TOK_LBRACE) {
			func_def->block = empty_block(parser);
			func_def->span  = span_span(start_tok->span, parser->cur->span);
			return func_def;
		}
	}

	func_def->block = parse_block(parser);
	func_def->span  = span_span(start_tok->span, func_def->block->span);

	return func_def;
}

static AstItem *parse_item(Parser *parser) {
	const Token *starting_token = parser->cur;

	AstItem *item = parser_alloc_one(parser, AstItem);

	switch (starting_token->kind) {
	case TOK_KEY_STRUCT:
		item->kind  = AST_ITEM_STRUCT;
		item->struc = parse_def_struct(parser);
		item->span  = item->struc->span;
		break;

	case TOK_KEY_PUB:
	case TOK_KEY_FN:
		item->kind     = AST_ITEM_FUNCTION;
		item->function = parse_def_func(parser);
		item->span     = item->function->span;
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
