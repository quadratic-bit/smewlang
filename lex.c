#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#include "vec.h"
#include "buf.h"

typedef struct {
	size_t start;
	size_t len;
} Span;

typedef enum {
	TOK_UNDEF,

	TOK_IDENTIFIER,

	TOK_LITERAL_INT,

	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_LBRACKET,
	TOK_RBRACKET,

	TOK_EQUAL,
	TOK_PLUS,
	TOK_MINUS,
	TOK_GT,
	TOK_LT,

	TOK_COMMA,
	TOK_COLON,
	TOK_DOT,
	TOK_QUESTION,
	TOK_SEMICOLON,
} TokenKind;

static const char *token_kind_name(TokenKind kind) {
	switch (kind) {
	case TOK_UNDEF:
		return "UNDEFINED";
	case TOK_IDENTIFIER:
		return "IDENTIFIER";
	case TOK_LITERAL_INT:
		return "LITERAL:INT";
	case TOK_LPAREN:
		return "PAREN:L";
	case TOK_RPAREN:
		return "PAREN:R";
	case TOK_LBRACKET:
		return "BRACKET:L";
	case TOK_RBRACKET:
		return "BRACKET:R";
	case TOK_LBRACE:
		return "BRACE:L";
	case TOK_RBRACE:
		return "BRACE:R";
	case TOK_EQUAL:
		return "EQUAL";
	case TOK_PLUS:
		return "PLUS";
	case TOK_MINUS:
		return "MINUS";
	case TOK_GT:
		return "GT";
	case TOK_LT:
		return "LT";
	case TOK_COMMA:
		return "COMMA";
	case TOK_COLON:
		return "COLON";
	case TOK_DOT:
		return "DOT";
	case TOK_QUESTION:
		return "QUESTION";
	case TOK_SEMICOLON:
		return "SEMICOLON";
	}
}

typedef struct {
	Span span;
	TokenKind kind;
} Token;

typedef Vec(Token) Tokens;

typedef struct {
	SourceBuffer *src;
	Tokens toks;
	size_t cur;
} Lexer;

static inline int lex_in_range(Lexer *lexer) {
	return lexer->cur < lexer->src->len;
}

static inline char lex_cur_char(Lexer *lexer) {
	assert(lex_in_range(lexer) && "Lexer cursor is out of source bounds");
	return lexer->src->data[lexer->cur];
}

static void consume_whitespace(Lexer *lexer) {
	while (lex_in_range(lexer) && isspace((unsigned char)lex_cur_char(lexer))) {
		lexer->cur++;
	}
}

static void consume_digits(Lexer *lexer) {
	while (lex_in_range(lexer) && isdigit((unsigned char)lex_cur_char(lexer))) {
		lexer->cur++;
	}
}

// start inclusive, end exclusive
static VecResult lex_emit(Lexer *lexer, TokenKind kind, size_t start, size_t end) {
	Token tok = {
		.kind = kind,
		.span = {
			.start = start,
			.len   = end - start,
		},
	};

	return vec_push(&lexer->toks, &tok);
}

static VecResult lex_literal_int(Lexer *lexer) {
	assert(lex_in_range(lexer));

	size_t tok_start = lexer->cur;
	consume_digits(lexer);

	return lex_emit(lexer, TOK_LITERAL_INT, tok_start, lexer->cur);
}

static inline int is_ident_ch(char ch) {
	unsigned char c = (unsigned char)ch;
	return isalnum(c) || c == '_';
}

static VecResult lex_identifier(Lexer *lexer) {
	assert(lex_in_range(lexer));

	size_t tok_start = lexer->cur;

	while (lex_in_range(lexer) && is_ident_ch(lex_cur_char(lexer))) {
		lexer->cur++;
	}

	return lex_emit(lexer, TOK_IDENTIFIER, tok_start, lexer->cur);
}

static VecResult consume_token(Lexer *lexer) {
	consume_whitespace(lexer);
	assert(lexer->cur <= lexer->src->len);
	if (lexer->cur == lexer->src->len) return VEC_OK;

	char cur_ch = lexer->src->data[lexer->cur];
	TokenKind kind = TOK_UNDEF;

	switch (cur_ch) {
	case '(':
		kind = TOK_LPAREN;
		break;
	case ')':
		kind = TOK_RPAREN;
		break;
	case '[':
		kind = TOK_LBRACKET;
		break;
	case ']':
		kind = TOK_RBRACKET;
		break;
	case '{':
		kind = TOK_LBRACE;
		break;
	case '}':
		kind = TOK_RBRACE;
		break;
	case ',':
		kind = TOK_COMMA;
		break;
	case '.':
		kind = TOK_DOT;
		break;
	case '?':
		kind = TOK_QUESTION;
		break;
	case ':':
		kind = TOK_COLON;
		break;
	case ';':
		kind = TOK_SEMICOLON;
		break;
	case '=':
		kind = TOK_EQUAL;
		break;
	case '+':
		kind = TOK_PLUS;
		break;
	case '-':
		kind = TOK_MINUS;
		break;
	case '>':
		kind = TOK_GT;
		break;
	case '<':
		kind = TOK_LT;
		break;
	}

	if (kind != TOK_UNDEF) {
		size_t tok_start = lexer->cur++;
		return lex_emit(lexer, kind, tok_start, lexer->cur);
	}

	if (isdigit((unsigned char)cur_ch)) {
		return lex_literal_int(lexer);
	} else if (is_ident_ch(cur_ch)) {
		kind = TOK_IDENTIFIER;
	} else {
		printf("%c (%d)\n", cur_ch, cur_ch);
		assert(0 && "Unimplemented");
	}

	return lex_identifier(lexer);
}

static Lexer lex(SourceBuffer *buf) {
	const size_t START_TOKENS_CAP = 128;

	Lexer lexer = {.src = buf, .cur = 0};
	vec_init(&lexer.toks, START_TOKENS_CAP);

	while (lexer.cur < lexer.src->len) {
		if (consume_token(&lexer) == VEC_ERR) {
			return lexer;
		}
	}

	return lexer;
}

static void lex_free(Lexer *lexer) {
	vec_free(&lexer->toks);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		if (argc == 0 || argv[0] == NULL)
			puts("usage: lex <filename>");
		else
			printf("usage: %s <filename>\n", argv[0]);

		return 1;
	}

	const char *input_filename = argv[1];
	FILE *input_file = fopen(input_filename, "rb");
	if (!input_file) {
		perror("main@fopen");
		return 1;
	}

	SourceBuffer source;
	vec_init(&source, BUFSIZ);

	if (!source.data) return 1;
	assert(source.cap > 0);
	assert(source.len == 0);

	if (buf_read(&source, input_file) == BUF_ERR) {
		vec_free(&source);
		fclose(input_file);
		return 1;
	}

	fwrite(source.data, 1, source.len, stdout);
	puts("\n");

	Lexer lexer = lex(&source);

	for (size_t i = 0; i < lexer.toks.len; ++i) {
		Token tok = lexer.toks.data[i];
		printf("%-12s %.*s\n", token_kind_name(tok.kind), (int)tok.span.len, lexer.src->data + tok.span.start);
	}

	lex_free(&lexer);
	vec_free(&source);
	fclose(input_file);
	return 0;
}
