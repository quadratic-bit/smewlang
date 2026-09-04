#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vec.h"

typedef Vec(char) SourceBuffer;

typedef struct {
	const char *src;
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
	TOK_COMMA,
	TOK_COLON,
	TOK_EQUAL,
	TOK_DOT,
	TOK_QUESTION,
	TOK_SEMICOLON,
} TokenKind;

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

static VecResult buf_read(SourceBuffer *buf, FILE *input_file) {
	size_t n_read_bytes = 0;

	do {
		size_t n_reserved_bytes = buf->cap - buf->len;
		if (n_reserved_bytes == 0) {
			if (buf->cap > SIZE_MAX - buf->cap) {
				fputs("Input file size is too big.\n", stderr);
				return VEC_ERR;
			}

			size_t new_buf_size = buf->cap * 2;
			if (vec_grow(buf, new_buf_size) == VEC_ERR) return VEC_ERR;

			n_reserved_bytes = buf->cap - buf->len;
		}
		n_read_bytes = fread(buf->data + buf->len, 1, n_reserved_bytes, input_file);
		buf->len += n_read_bytes;
	} while (n_read_bytes > 0);

	if (ferror(input_file)) {
		perror("buf_read@fread");
		return VEC_ERR;
	}

	return VEC_OK;
}

static void consume_whitespace(Lexer *lexer) {
	while (lexer->cur < lexer->src->len && isspace(lexer->src->data[lexer->cur])) {
		lexer->cur++;
	}
}

static VecResult consume_token(Lexer *lexer) {
	consume_whitespace(lexer);
	assert(lexer->cur <= lexer->src->len);
	if (lexer->cur == lexer->src->len) return VEC_OK;

	size_t tok_start = lexer->cur;
	char   cur_ch    = lexer->src->data[lexer->cur++];
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

	// literal or identifier
	default: break;
	}

	if (kind != TOK_UNDEF) {
		Token tok = {
			.kind = kind,
			.span = {
				.src = lexer->src->data + tok_start,
				.len = 1
			}
		};
		if (vec_push(&lexer->toks, &tok) == VEC_ERR) return VEC_ERR;
		return VEC_OK;
	}

	while (lexer->cur < lexer->src->len && (isalnum(lexer->src->data[lexer->cur]) || lexer->src->data[lexer->cur] == '_')) {
		lexer->cur++;
	}

	Token tok = {
		.kind = TOK_IDENTIFIER,
		.span = {
			.src = lexer->src->data + tok_start,
			.len = lexer->cur - tok_start
		}
	};
	if (vec_push(&lexer->toks, &tok) == VEC_ERR) return VEC_ERR;
	return VEC_OK;
}

static Lexer lex(SourceBuffer *buf) {
	const size_t START_TOKENS_CAP = 128;

	Lexer lexer = {.src = buf, .cur = 0};
	vec_init(&lexer.toks, START_TOKENS_CAP);

	while (lexer.cur < lexer.src->len) {
		consume_token(&lexer);
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

	if (buf_read(&source, input_file) == VEC_ERR) {
		vec_free(&source);
		fclose(input_file);
		return 1;
	}

	fwrite(source.data, 1, source.len, stdout);
	puts("\n");

	Lexer lexer = lex(&source);

	for (size_t i = 0; i < lexer.toks.len; ++i) {
		Token tok = lexer.toks.data[i];
		printf("%d %.*s\n", tok.kind, (int)tok.span.len, tok.span.src);
	}

	lex_free(&lexer);
	vec_free(&source);
	fclose(input_file);
	return 0;
}
