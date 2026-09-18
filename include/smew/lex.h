#ifndef LEX_H
#define LEX_H

#include <smew/diag.h>
#include <smew/source.h>
#include <smew/vec.h>

#include <stddef.h>

typedef enum {
	TOK_UNK,

	TOK_IDENTIFIER,

	TOK_KEY_PUB,
	TOK_KEY_FN,
	TOK_KEY_IN,
	TOK_KEY_WITH,
	TOK_KEY_MUT,
	TOK_KEY_LOOP,
	TOK_KEY_IF,
	TOK_KEY_ELSE,
	TOK_KEY_STRUCT,
	TOK_KEY_RETURN,
	TOK_KEY_MOVE,
	TOK_KEY_LET,

	TOK_LITERAL_INT,
	TOK_LITERAL_STRING,

	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_LBRACKET,
	TOK_RBRACKET,

	TOK_PLUS,
	TOK_MINUS,
	TOK_PERCENT,
	TOK_SLASH,
	TOK_STAR,
	TOK_BANG,
	TOK_ASSIGN,
	TOK_PLUS_ASSIGN,
	TOK_MINUS_ASSIGN,

	TOK_GT,
	TOK_LT,

	TOK_GE,
	TOK_LE,

	TOK_EQUAL,
	TOK_NOT_EQUAL,
	TOK_AND,
	TOK_OR,

	TOK_PIPE,
	TOK_AMP,
	TOK_HAT,

	TOK_ARROW,

	TOK_COMMA,
	TOK_COLON,
	TOK_DOT,
	TOK_QUESTION,
	TOK_SEMICOLON,

	TOK_EOF
} TokenKind;

typedef struct {
	Span      span;
	TokenKind kind;
} Token;

typedef Vec(Token) Tokens;

typedef struct {
	SourceFile *src;
	size_t      cur;

	Tokens toks;
	Diags  diags;
} Lexer;

Lexer lex(SourceFile *src);

void lex_free(Lexer *lexer);

const char *token_kind_name(TokenKind kind);

#endif
