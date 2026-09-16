#ifndef PARSE_INTERNAL_H
#define PARSE_INTERNAL_H

#include <smew/arena.h>
#include <smew/parse.h>

#include <stdalign.h>
#include <stddef.h>

static const uint8_t LOWEST_BP = 0;

typedef struct {
	uint8_t left;
	uint8_t right;
} BindingPower;

void *arena_alloc_guarded(Arena *arena, size_t size, size_t align);

#define parser_alloc_one(parser, type) \
	((type *)arena_alloc_guarded(&(parser)->arena, sizeof(type), alignof(type)))

void      consume          (Parser *parser, TokenKind expect);
int       consume_or_insert(Parser *parser, TokenKind expect, const char *expect_str);
AstIdent *consume_ident    (Parser *parser);

AstType *unknown_type(Parser *parser);

AstType *parse_type(Parser *parser, uint8_t ambient_bp);
AstExpr *parse_expr(Parser *parser, uint8_t ambient_bp);

void add_diag_expected(Parser *parser, ParseDiagKind kind, Span span, const char *expect);

#endif
