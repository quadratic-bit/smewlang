#include <smew/lex.h>

#include <smew/buf.h>
#include <smew/colors.h>
#include <smew/line.h>
#include <smew/vec.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef enum {
	LEX_OK,
	LEX_ERR_OOM,
} LexResult;

const char *token_kind_name(TokenKind kind) {
	switch (kind) {
	case TOK_UNK:
		return "UNKNOWN";
	case TOK_IDENTIFIER:
		return "IDENTIFIER";
	case TOK_LITERAL_INT:
		return "LITERAL:INT";
	case TOK_LITERAL_STRING:
		return "LITERAL:STRING";
	case TOK_KEY_PUB:
		return "KEYWORD:PUB";
	case TOK_KEY_FN:
		return "KEYWORD:FN";
	case TOK_KEY_IN:
		return "KEYWORD:IN";
	case TOK_KEY_WITH:
		return "KEYWORD:WITH";
	case TOK_KEY_MUT:
		return "KEYWORD:MUT";
	case TOK_KEY_LOOP:
		return "KEYWORD:LOOP";
	case TOK_KEY_IF:
		return "KEYWORD:IF";
	case TOK_KEY_ELSE:
		return "KEYWORD:ELSE";
	case TOK_KEY_STRUCT:
		return "KEYWORD:STRUCT";
	case TOK_KEY_RETURN:
		return "KEYWORD:RETURN";
	case TOK_KEY_MOVE:
		return "KEYWORD:MOVE";
	case TOK_KEY_LET:
		return "KEYWORD:LET";
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
	case TOK_ASSIGN:
		return "ASSIGN";
	case TOK_PLUS:
		return "PLUS";
	case TOK_MINUS:
		return "MINUS";
	case TOK_PERCENT:
		return "PERCENT";
	case TOK_SLASH:
		return "SLASH";
	case TOK_STAR:
		return "STAR";
	case TOK_BANG:
		return "BANG";
	case TOK_PLUS_ASSIGN:
		return "ASSIGN:PLUS";
	case TOK_MINUS_ASSIGN:
		return "ASSIGN:MINUS";
	case TOK_EQUAL:
		return "EQUAL";
	case TOK_NOT_EQUAL:
		return "EQUAL:NOT";
	case TOK_GT:
		return "GT";
	case TOK_LT:
		return "LT";
	case TOK_GE:
		return "GE";
	case TOK_LE:
		return "LE";
	case TOK_AND:
		return "AND";
	case TOK_OR:
		return "OR";
	case TOK_PIPE:
		return "PIPE";
	case TOK_AMP:
		return "AMP";
	case TOK_HAT:
		return "HAT";
	case TOK_ARROW:
		return "ARROW";
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
	case TOK_EOF:
		return "EOF";
	}
}



// XXX: fails silently
static void add_diag(Lexer *lexer, LexDiagKind kind, Span span) {
	LexDiag diag = {.kind = kind, .span = span, .expected = ""};
	vec_push(&lexer->diags, &diag);
}

static const char *diag_message(LexDiag *diag) {
	switch (diag->kind) {
	case LEX_DIAG_INVALID_IDENTIFIER:
		return "Identifier must not start with a digit";
	case LEX_DIAG_UNKNOWN_CHARACTER:
		return "Encountered unsupported symbol";
	case LEX_DIAG_UNCLOSED_STRING_LITERAL:
		return "Unclosed string literal";
	}
}

void print_lex_diag(Lexer *lexer, LexDiag *diag) {
	print_diag(lexer->filename, lexer->src, diag->span, diag_message(diag), diag->expected);
}

static inline int cur_in_range(Lexer *lexer) {
	return lexer->cur < lexer->src->len;
}

static inline int is_ident_continue(char ch) {
	unsigned char c = (unsigned char)ch;
	return isalnum(c) || c == '_';
}

static inline int is_ident_start(char ch) {
	unsigned char c = (unsigned char)ch;
	return isalpha(c) || c == '_';
}

static inline char cur_lexer_ch(Lexer *lexer) {
	assert(cur_in_range(lexer) && "Lexer cursor is out of source bounds");
	return lexer->src->data[lexer->cur];
}

static inline char peek_lexer_ch(Lexer *lexer, size_t offset) {
	if (lexer->cur + offset >= lexer->src->len) {
		return '\0';
	}
	return lexer->src->data[lexer->cur + offset];
}

static void consume_whitespace(Lexer *lexer) {
	while (cur_in_range(lexer) && isspace((unsigned char)cur_lexer_ch(lexer))) {
		lexer->cur++;
	}
}

static void consume_digits(Lexer *lexer) {
	while (cur_in_range(lexer) && isdigit((unsigned char)cur_lexer_ch(lexer))) {
		lexer->cur++;
	}
}

static void consume_string_body(Lexer *lexer) {
	while (cur_in_range(lexer) && cur_lexer_ch(lexer) != '"' && cur_lexer_ch(lexer) != '\n') {
		lexer->cur++;
	}
}

static void consume_ident_remaining(Lexer *lexer) {
	while (cur_in_range(lexer) && is_ident_continue(cur_lexer_ch(lexer))) {
		lexer->cur++;
	}
}

// start inclusive, end exclusive
static LexResult lex_emit(Lexer *lexer, TokenKind kind, size_t start, size_t end) {
	Token tok = {
		.kind = kind,
		.span = {
			.start = start,
			.len   = end - start,
		},
	};

	if (vec_push(&lexer->toks, &tok) != VEC_OK) {
		return LEX_ERR_OOM;
	}

	return LEX_OK;
}

static Span span_from(Lexer *lexer, size_t start) {
	assert(lexer->cur > start && "Cannot start span from future");
	return (Span){
		.start = start,
		.len   = lexer->cur - start,
	};
}

static LexResult lex_literal_int(Lexer *lexer) {
	assert(cur_in_range(lexer));
	assert(isdigit((unsigned char)cur_lexer_ch(lexer)) && "Expected to be on a digit");

	size_t tok_start = lexer->cur++;
	consume_digits(lexer);

	if (cur_in_range(lexer) && is_ident_continue(cur_lexer_ch(lexer))) {
		// Invalid identifier (starts with digits). Parse the remaining part then recover
		consume_ident_remaining(lexer);
		add_diag(lexer, LEX_DIAG_INVALID_IDENTIFIER, span_from(lexer, tok_start));
		return lex_emit(lexer, TOK_UNK, tok_start, lexer->cur);
	}

	return lex_emit(lexer, TOK_LITERAL_INT, tok_start, lexer->cur);
}

static LexResult lex_literal_string(Lexer *lexer) {
	assert(cur_in_range(lexer));
	assert(cur_lexer_ch(lexer) == '"' && "Expected to be on a quotation mark");

	size_t tok_start = lexer->cur++;
	consume_string_body(lexer);

	if (!cur_in_range(lexer) || cur_lexer_ch(lexer) == '\n') {
		// Unclosed string literal
		add_diag(lexer, LEX_DIAG_UNCLOSED_STRING_LITERAL, span_from(lexer, tok_start));
		return lex_emit(lexer, TOK_UNK, tok_start, lexer->cur);
	}
	assert(cur_in_range(lexer));
	assert(cur_lexer_ch(lexer) == '"');

	lexer->cur++;  // <- consume remaining quotationg mark

	return lex_emit(lexer, TOK_LITERAL_STRING, tok_start, lexer->cur);
}

static int compare_span(const char *token, size_t len, const char *ref) {
	const size_t ref_len = strlen(ref);
	if (ref_len != len) return 0;
	return !memcmp(token, ref, len);
}

// PERF: a perfect hashtable would do better
static TokenKind try_keyword_cast(Lexer *lexer, size_t start) {
	size_t end = lexer->cur; // exclusive
	assert(end > start);
	size_t tok_len = end - start;
	const char *tok = lexer->src->data + start;
	if (compare_span(tok, tok_len, "pub"   )) return TOK_KEY_PUB;
	if (compare_span(tok, tok_len, "fn"    )) return TOK_KEY_FN;
	if (compare_span(tok, tok_len, "in"    )) return TOK_KEY_IN;
	if (compare_span(tok, tok_len, "struct")) return TOK_KEY_STRUCT;
	if (compare_span(tok, tok_len, "return")) return TOK_KEY_RETURN;
	if (compare_span(tok, tok_len, "move"  )) return TOK_KEY_MOVE;
	if (compare_span(tok, tok_len, "with"  )) return TOK_KEY_WITH;
	if (compare_span(tok, tok_len, "mut"   )) return TOK_KEY_MUT;
	if (compare_span(tok, tok_len, "loop"  )) return TOK_KEY_LOOP;
	if (compare_span(tok, tok_len, "if"    )) return TOK_KEY_IF;
	if (compare_span(tok, tok_len, "else"  )) return TOK_KEY_ELSE;
	if (compare_span(tok, tok_len, "let"   )) return TOK_KEY_LET;
	return TOK_IDENTIFIER;
}

static LexResult lex_identifier(Lexer *lexer) {
	assert(cur_in_range(lexer));
	assert(is_ident_start(cur_lexer_ch(lexer)) && "Expected to be on a valid identifier start");

	size_t tok_start = lexer->cur++;
	consume_ident_remaining(lexer);

	return lex_emit(lexer, try_keyword_cast(lexer, tok_start), tok_start, lexer->cur);
}

static TokenKind match_sym(Lexer *lexer, char sym, TokenKind match, TokenKind miss) {
	if (peek_lexer_ch(lexer, 1) == sym) {
		lexer->cur++;
		return match;
	}
	return miss;
}

static TokenKind match_sym2(Lexer *lexer, char sym1, char sym2, TokenKind match1, TokenKind match2,
                            TokenKind miss)
{
	if (peek_lexer_ch(lexer, 1) == sym1) {
		lexer->cur++;
		return match1;
	}
	if (peek_lexer_ch(lexer, 1) == sym2) {
		lexer->cur++;
		return match2;
	}
	return miss;
}

static LexResult consume_token(Lexer *lexer) {
	consume_whitespace(lexer);
	assert(lexer->cur <= lexer->src->len);
	if (lexer->cur == lexer->src->len) return LEX_OK;

	size_t tok_start = lexer->cur;
	char cur_ch = lexer->src->data[lexer->cur];
	TokenKind kind = TOK_UNK;
	int is_comment = 0;

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
		kind = match_sym(lexer, '=', TOK_EQUAL, TOK_ASSIGN);
		break;
	case '+':
		kind = match_sym(lexer, '=', TOK_PLUS_ASSIGN, TOK_PLUS);
		break;
	case '-':
		kind = match_sym2(lexer, '=', '>', TOK_MINUS_ASSIGN, TOK_ARROW, TOK_MINUS);
		break;
	case '%':
		kind = TOK_PERCENT;
		break;
	case '/':
		if (peek_lexer_ch(lexer, 1) == '/') is_comment = 1;
		kind = TOK_SLASH;
		break;
	case '*':
		kind = TOK_STAR;
		break;
	case '!':
		kind = match_sym(lexer, '=', TOK_NOT_EQUAL, TOK_BANG);
		break;
	case '>':
		kind = match_sym(lexer, '=', TOK_GE, TOK_GT);
		break;
	case '<':
		kind = match_sym(lexer, '=', TOK_LE, TOK_LT);
		break;
	case '|':
		kind = match_sym(lexer, '|', TOK_OR, TOK_PIPE);
		break;
	case '&':
		kind = match_sym(lexer, '&', TOK_AND, TOK_AMP);
		break;
	case '^':
		kind = TOK_HAT;
		break;
	}

	if (is_comment) {
		lexer->cur += 2;
		while (cur_in_range(lexer) && cur_lexer_ch(lexer) != '\n') {
			lexer->cur++;
		}
		return LEX_OK;
	}

	if (kind != TOK_UNK) {
		return lex_emit(lexer, kind, tok_start, ++lexer->cur);
	}

	if (cur_ch == '"') {
		return lex_literal_string(lexer);
	}

	if (isdigit((unsigned char)cur_ch))
	{
		return lex_literal_int(lexer);
	}

	if (is_ident_start(cur_ch))
	{
		return lex_identifier(lexer);
	}

	lexer->cur++;
	add_diag(lexer, LEX_DIAG_UNKNOWN_CHARACTER, span_from(lexer, tok_start));
	return lex_emit(lexer, TOK_UNK, tok_start, lexer->cur);
}

Lexer lex(SourceBuffer *buf, const char *filename) {
	const size_t START_TOKENS_CAP = 128;

	Lexer lexer = {.filename = filename, .src = buf, .cur = 0};
	vec_init(&lexer.toks,  START_TOKENS_CAP);
	vec_init(&lexer.diags, 1);

	while (cur_in_range(&lexer)) {
		if (consume_token(&lexer) != LEX_OK) {
			return lexer;
		}
	}

	lex_emit(&lexer, TOK_EOF, lexer.cur, lexer.cur);
	return lexer;
}

void lex_free(Lexer *lexer) {
	vec_free(&lexer->toks);
	vec_free(&lexer->diags);
}
