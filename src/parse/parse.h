#ifndef PARSE_PARSE_H
#define PARSE_PARSE_H

#include <smew/ast.h>
#include <smew/arena.h>
#include <smew/lex.h>
#include <smew/parse.h>

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>


static const uint8_t MIN_BP = 0;
static const uint8_t NO_BP  = MIN_BP;

typedef struct {
	uint8_t left;
	uint8_t right;
} BindingPower;

void *arena_alloc_guarded(Arena *arena, size_t size, size_t align);

#define parser_alloc_one(parser, type) \
	((type *)arena_alloc_guarded(&(parser)->arena, sizeof(type), alignof(type)))

const Token *prev(const Parser *parser);

int guard_eof(Parser *parser);

void      consume          (Parser *parser, TokenKind expect);
int       consume_or_insert(Parser *parser, TokenKind expect, const char *expect_str);
AstIdent *consume_ident    (Parser *parser);

#endif
