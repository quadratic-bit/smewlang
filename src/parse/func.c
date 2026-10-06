#include "func.h"
#include "block.h"
#include "parse.h"
#include "type.h"

#include <assert.h>

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

static AstFunctionContext *parse_func_contexts(Parser *parser) {
	AstFunctionContext *ctx = parser_alloc_one(parser, AstFunctionContext);
	ctx->next = NULL;

	if (parser->cur->kind == TOK_IDENTIFIER) {
		ctx->name = consume_ident(parser);
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");
		ctx->name = unknown_ident(parser);
	}

	if (parser->cur->kind == TOK_COMMA) {
		consume(parser, TOK_COMMA);
		ctx->next = parse_func_contexts(parser);
	}
	ctx->span = ctx->name->span;

	return ctx;
}

static AstFunctionGeneric *parse_func_generics(Parser *parser) {
	AstFunctionGeneric *generic = parser_alloc_one(parser, AstFunctionGeneric);
	generic->next = NULL;

	if (parser->cur->kind == TOK_IDENTIFIER) {
		generic->name = consume_ident(parser);
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");
		generic->name = unknown_ident(parser);
	}

	if (parser->cur->kind == TOK_COMMA) {
		consume(parser, TOK_COMMA);
		generic->next = parse_func_generics(parser);
	}
	generic->span = generic->name->span;

	return generic;
}

AstFunction *parse_def_func(Parser *parser) {
	assert((parser->cur->kind == TOK_KEY_PUB ||
	        parser->cur->kind == TOK_KEY_FN) && "Function must start with `fn` or `pub");

	const Token *start_tok = parser->cur;

	AstFunctionDeclaration *func_decl = parser_alloc_one(parser, AstFunctionDeclaration);
	AstFunction            *func_def  = parser_alloc_one(parser, AstFunction);

	func_def->decl = func_decl;

	func_decl->params   = NULL;
	func_decl->generics = NULL;
	func_decl->contexts = NULL;

	func_decl->is_public = consume_maybe(parser, TOK_KEY_PUB);
	consume_or_insert(parser, TOK_KEY_FN, "`fn` keyword");

	if (parser->cur->kind == TOK_IDENTIFIER) {
		func_decl->name = consume_ident(parser);
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");

		func_decl->name = unknown_ident(parser);

		while (parser->cur->kind != TOK_LPAREN &&
		       parser->cur->kind != TOK_LBRACE &&
		       parser->cur->kind != TOK_EOF) {
			parser->cur++;
		}

		if (guard_eof(parser)) {
			func_def ->span        = span_span(start_tok->span, prev(parser)->span);
			func_def ->def         = empty_block (parser);
			func_decl->return_type = unknown_type(parser);
			return func_def;
		}
	}

	if (parser->cur->kind == TOK_LBRACKET) {
		consume(parser, TOK_LBRACKET);
		if (parser->cur->kind != TOK_RBRACKET) {
			func_decl->generics = parse_func_generics(parser);
			consume_or_insert(parser, TOK_RBRACKET, "closing bracket");
		} else {
			consume(parser, TOK_RBRACKET);
		}
	}
	if (parser->cur->kind == TOK_LPAREN) {
		consume(parser, TOK_LPAREN);

		if (parser->cur->kind != TOK_RPAREN) {
			func_decl->params = parse_func_params(parser);
		}

		consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "opening parenthesis (function params)");
	}

	if (parser->cur->kind == TOK_KEY_IN) {
		consume(parser, TOK_KEY_IN);
		func_decl->contexts = parse_func_contexts(parser);
	}

	consume_or_insert(parser, TOK_ARROW, "arrow");

	func_decl->return_type = parse_type(parser, MIN_BP);

	if (parser->cur->kind != TOK_LBRACE) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token after function declaration", "opening brace");
		while (parser->cur->kind != TOK_LBRACE && !is_item_start(parser->cur->kind)) {
			parser->cur++;
		}
		if (parser->cur->kind != TOK_LBRACE) {
			func_def->def  = empty_block(parser);
			func_def->span = span_span(start_tok->span, parser->cur->span);
			return func_def;
		}
	}

	func_def->def  = parse_block(parser);
	func_def->span = span_span(start_tok->span, func_def->def->span);

	return func_def;
}

