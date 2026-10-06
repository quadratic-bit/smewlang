#include "trait.h"

#include "func.h"
#include "parse.h"
#include "smew/lex.h"

#include <smew/ast.h>
#include <smew/diag.h>
#include <smew/parse.h>
#include <smew/source.h>

#include <assert.h>

AstTrait *parse_trait(Parser *parser) {
	assert(parser->cur->kind == TOK_KEY_TRAIT && "Unexpected token kind");
	consume(parser, TOK_KEY_TRAIT);

	AstTrait *trait = parser_alloc_one(parser, AstTrait);
	trait->funcs = NULL;
	trait->reqs  = NULL;

	if (parser->cur->kind == TOK_IDENTIFIER) {
		trait->name = consume_ident(parser);
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");
		trait->name = unknown_ident(parser);

		if (parser->cur->kind != TOK_LBRACE) {
			parser->cur++;  // maybe it's a keyword or a literal? TODO: repeated pattern
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
			last = &(*last)->next;
		}

		if (parser->cur->kind == TOK_LBRACE) {
			parse_func_def_with_decl(parser, (*last)->func);
		}

		consume_or_insert(parser, TOK_SEMICOLON, "semicolon");
	}

	if (guard_eof(parser)) {
		return trait;
	}

	consume(parser, TOK_RBRACE);
	return trait;
}
