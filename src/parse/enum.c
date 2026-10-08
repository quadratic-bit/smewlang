#include "enum.h"

#include "func.h"
#include "parse.h"
#include "smew/source.h"
#include "type.h"

#include <smew/lex.h>

#include <assert.h>


AstEnum *parse_enum(Parser *parser) {
	assert(parser->cur->kind == TOK_KEY_ENUM && "Unexpected token kind");
	consume(parser, TOK_KEY_ENUM);

	AstEnum *enumer = parser_alloc_one(parser, AstEnum);
	enumer->generics = NULL;
	enumer->members  = NULL;

	if (parser->cur->kind == TOK_IDENTIFIER) {
		enumer->name = consume_ident(parser);
	} else {
		add_diag_expected(&parser->diags, parser->cur->span,
		                  "Unexpected token", "identifier");
		enumer->name = unknown_ident(parser);

		if (parser->cur->kind != TOK_LBRACE && parser->cur->kind != TOK_LBRACKET) {
			parser->cur++;  // maybe it's a keyword or a literal? TODO: repeated pattern
		}
	}

	if (parser->cur->kind == TOK_LBRACKET) {
		consume(parser, TOK_LBRACKET);
		enumer->generics = parse_generic_list(parser);
		consume_or_insert(parser, TOK_RBRACKET, "closing bracket");
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
			enumer->span = zero_span();
			return enumer;
		}
	}

	consume(parser, TOK_LBRACE);

	AstEnumMember **last = &enumer->members;

	while (parser->cur->kind != TOK_RBRACE && parser->cur->kind != TOK_EOF) {
		const Token *variant_start = parser->cur;

		*last = parser_alloc_one(parser, AstEnumMember);

		(*last)->payload = NULL;
		(*last)->next    = NULL;

		if (parser->cur->kind == TOK_IDENTIFIER) {
			(*last)->name = consume_ident(parser);
		} else {
			add_diag_expected(&parser->diags, parser->cur->span,
					  "Unexpected token", "identifier");
			(*last)->name = unknown_ident(parser);

			if (parser->cur->kind != TOK_LPAREN) {
				parser->cur++;  // maybe it's a keyword or a literal?
			}
		}

		if (parser->cur->kind == TOK_LPAREN) {
			consume(parser, TOK_LPAREN);
			(*last)->payload = parse_type_list(parser);
			consume_or_insert(parser, TOK_RPAREN, "closing parenthesis");
		}

		(*last)->span = span_span(variant_start->span, parser->cur->span);

		last = &(*last)->next;

		if (parser->cur->kind == TOK_RBRACE || parser->cur->kind == TOK_EOF) break;

		consume_or_insert(parser, TOK_COMMA, "comma");

	}

	if (guard_eof(parser)) {
		return enumer;
	}

	consume(parser, TOK_RBRACE);
	return enumer;
}
