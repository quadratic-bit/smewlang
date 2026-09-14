#ifndef LEX_H
#define LEX_H

#include <smew/buf.h>
#include <smew/diag.h>
#include <smew/line.h>
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

typedef enum {
	LEX_DIAG_INVALID_IDENTIFIER,
	LEX_DIAG_UNKNOWN_CHARACTER,
	LEX_DIAG_UNCLOSED_STRING_LITERAL
} LexDiagKind;

typedef Diag(LexDiagKind) LexDiag;

typedef Vec(LexDiag) LexDiags;

typedef struct {
	const char *filename;

	SourceBuffer *src;
	size_t        cur;

	Tokens   toks;
	LexDiags diags;
} Lexer;

Lexer lex(SourceBuffer *buf, const char *filename);

void lex_free(Lexer *lexer);

const char *token_kind_name(TokenKind kind);

void print_lex_diag(Lexer *lexer, LexDiag *diag);

#endif
