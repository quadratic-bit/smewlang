#include "trait.h"

#include "func.h"
#include "parse.h"
#include "type.h"

#include <smew/ast.h>
#include <smew/diag.h>
#include <smew/lex.h>
#include <smew/parse.h>
#include <smew/source.h>

#include <assert.h>

AstTrait *parse_trait(Parser *parser) {
	assert(parser->cur->kind == TOK_KEY_TRAIT && "Unexpected token kind");
	consume(parser, TOK_KEY_TRAIT);

	AstTrait *trait = parser_alloc_one(parser, AstTrait);
	trait->funcs = NULL;
	trait->reqs  = NULL;

	trait->name = parse_required_ident(parser, "Unexpected token in trait declaration",
	                                   TOK_LBRACE, TOK_EOF);

	if (parser->cur->kind == TOK_COLON) {
		consume(parser, TOK_COLON);
		AstTraitReq **tail = &trait->reqs;
		while (parser->cur->kind == TOK_IDENTIFIER) {
			*tail = parser_alloc_one(parser, AstTraitReq);
			(*tail)->name = consume_ident(parser);
			(*tail)->next = NULL;
			tail = &(*tail)->next;
			if (parser->cur->kind != TOK_COMMA) {
				break;
			}
			consume(parser, TOK_COMMA);
		}
	}

	if (parser->cur->kind != TOK_LBRACE) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "opening brace");

		while (parser->cur->kind != TOK_LBRACE &&
		       parser->cur->kind != TOK_RBRACE &&
		       parser->cur->kind != TOK_EOF)
		{
			parser->cur++;
		}

		if (parser->cur->kind != TOK_LBRACE) {
			trait->span = zero_span();
			return trait;
		}
	}

	consume(parser, TOK_LBRACE);

	AstTraitFunction **last = &trait->funcs;

	while (parser->cur->kind != TOK_RBRACE && parser->cur->kind != TOK_EOF) {
		(*last)             = parser_alloc_one(parser, AstTraitFunction);
		(*last)->func       = parser_alloc_one(parser, AstFunction);
		(*last)->func->decl = parse_func_decl (parser);
		(*last)->func->def  = NULL;
		(*last)->next = NULL;

		if (parser->cur->kind == TOK_SEMICOLON) {
			(*last)->func->span = (*last)->func->decl->span;
		}

		if (parser->cur->kind == TOK_LBRACE) {
			parse_func_def_with_decl(parser, (*last)->func);
		}

		last = &(*last)->next;
		consume_or_insert(parser, TOK_SEMICOLON, "semicolon");
	}

	if (guard_eof(parser)) {
		return trait;
	}

	consume(parser, TOK_RBRACE);
	return trait;
}

AstImpl *parse_impl(Parser *parser) {
	assert(parser->cur->kind == TOK_KEY_IMPL && "Unexpected token kind");
	consume(parser, TOK_KEY_IMPL);

	AstImpl *impl = parser_alloc_one(parser, AstImpl);
	impl->generics  = NULL;
	impl->funcs     = NULL;
	impl->self_args = NULL;
	impl->trait     = NULL;

	if (parser->cur->kind == TOK_LBRACKET) {
		consume(parser, TOK_LBRACKET);
		impl->generics = parse_generic_list(parser);
		consume_or_insert(parser, TOK_RBRACKET, "closing bracket");
	}

	impl->self = parse_required_ident(parser, "Unexpected token", TOK_LPAREN, TOK_COLON);

	if (parser->cur->kind == TOK_LPAREN) {
		consume(parser, TOK_LPAREN);
		impl->self_args = parse_type_list(parser);
		consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");
	}

	if (parser->cur->kind == TOK_COLON) {
		consume(parser, TOK_COLON);

		impl->trait = parse_required_ident(parser, "Unexpected token", TOK_LBRACE, TOK_EOF);
	}

	if (parser->cur->kind != TOK_LBRACE) {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "opening brace");

		while (parser->cur->kind != TOK_LBRACE &&
		       parser->cur->kind != TOK_RBRACE &&
		       parser->cur->kind != TOK_EOF)
		{
			parser->cur++;
		}

		if (parser->cur->kind != TOK_LBRACE) {
			impl->span = zero_span();
			return impl;
		}
	}

	consume(parser, TOK_LBRACE);

	AstImplFunction **last = &impl->funcs;

	while (parser->cur->kind != TOK_RBRACE && parser->cur->kind != TOK_EOF) {
		(*last)             = parser_alloc_one(parser, AstImplFunction);
		(*last)->func       = parser_alloc_one(parser, AstFunction);
		(*last)->func->decl = parse_func_decl (parser);
		(*last)->func->def  = NULL;
		(*last)->next = NULL;

		if (parser->cur->kind == TOK_LBRACE) {
			parse_func_def_with_decl(parser, (*last)->func);
		} else {
			(*last)->func->span = (*last)->func->decl->span;
			add_diag_expected(&parser->diags, parser->cur->span,
			                  "Unexpected token", "function definition");
		}

		last = &(*last)->next;
		consume_or_insert(parser, TOK_SEMICOLON, "semicolon");
	}

	if (guard_eof(parser)) {
		return impl;
	}

	consume(parser, TOK_RBRACE);
	return impl;
}
