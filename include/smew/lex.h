#ifndef LEX_H
#define LEX_H

#include <smew/diag.h>
#include <smew/source.h>
#include <smew/vec.h>

#include <stddef.h>

typedef enum {
	TOK_UNK,

	TOK_IDENTIFIER,     // name

	TOK_KEY_PUB,        // pub
	TOK_KEY_FN,         // fn
	TOK_KEY_IN,         // in
	TOK_KEY_TRAIT,      // trait
	TOK_KEY_ENUM,       // enum
	TOK_KEY_WITH,       // with
	TOK_KEY_MUT,        // mut
	TOK_KEY_LOOP,       // loop
	TOK_KEY_BREAK,      // break
	TOK_KEY_IF,         // if
	TOK_KEY_ELSE,       // else
	TOK_KEY_STRUCT,     // struct
	TOK_KEY_RETURN,     // return
	TOK_KEY_MOVE,       // move
	TOK_KEY_LET,        // let
	TOK_KEY_UNIT,       // unit

	TOK_LITERAL_INT,    // 42
	TOK_LITERAL_STRING, // "str"

	TOK_LPAREN,         // (
	TOK_RPAREN,         // )
	TOK_LBRACE,         // {
	TOK_RBRACE,         // }
	TOK_LBRACKET,       // [
	TOK_RBRACKET,       // ]

	TOK_DOT_LBRACE,     // .{
	TOK_COLON_LBRACKET, // :[

	TOK_PLUS,           // +
	TOK_MINUS,          // -
	TOK_PERCENT,        // %
	TOK_SLASH,          // /
	TOK_STAR,           // *
	TOK_BANG,           // !
	TOK_ASSIGN,         // =
	TOK_PLUS_ASSIGN,    // +=
	TOK_MINUS_ASSIGN,   // -=

	TOK_GT,             // >
	TOK_LT,             // <

	TOK_GE,             // >=
	TOK_LE,             // <=

	TOK_EQUAL,          // ==
	TOK_NOT_EQUAL,      // !=
	TOK_AND,            // &&
	TOK_OR,             // ||

	TOK_PIPE,           // |
	TOK_AMP,            // &
	TOK_HAT,            // ^

	TOK_ARROW,          // ->

	TOK_COMMA,          // ,
	TOK_COLON,          // :
	TOK_DOT,            // .
	TOK_QUESTION,       // ?
	TOK_SEMICOLON,      // ;

	TOK_EOF
} TokenKind;

typedef struct {
	Span      span;
	TokenKind kind;
} Token;

typedef Vec(Token) Tokens;

typedef struct {
	const SourceFile *src;
	size_t cur;

	Tokens toks;
	Diags  diags;
} Lexer;

Lexer lex(const SourceFile *src);

void print_tokens(const Lexer *lexer);

void lex_free(Lexer *lexer);

const char *token_kind_name(TokenKind kind);

#endif
