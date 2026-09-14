#include <smew/parse.h>

#include <smew/arena.h>
#include <smew/colors.h>
#include <smew/lex.h>
#include <smew/line.h>
#include <smew/vec.h>

#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void *_arena_alloc_raw(Arena *arena, size_t size, size_t align) {
	void *mem = arena_alloc(arena, size, align);
	if (mem == NULL) {
		fprintf(stderr, "fatal: out of memory");
		exit(EXIT_FAILURE);
	}
	return mem;
}

#define parser_alloc_one(parser, type) \
	((type *)_arena_alloc_raw(&(parser)->arena, sizeof(type), alignof(type)))

// TODO: tweak
static const size_t DEFAULT_AST_ITEMS_CAP = 8;

static void consume(Parser *parser, TokenKind expected) {
	assert(parser->cur->kind == expected && "Unexpected token kind");
	parser->cur++;
}

// XXX: fails silently
static void add_diag(Parser *parser, ParseDiagKind kind, Span span) {
	ParseDiag diag = {.kind = kind, .span = span};
	vec_push(&parser->diags, &diag);
}

static const char *diag_message(ParseDiag *diag) {
	switch (diag->kind) {
	case AST_DIAG_UNEXPECTED_EOF:
		return "Unexpected EOF";
	}
}

void print_ast_diag(Parser *parser, SourceBuffer *src, ParseDiag *diag) {
	print_diag(parser->filename, src, diag_message(diag), diag->span);
}

static int guard_eof(Parser *parser) {
	if (parser->cur->kind != TOK_EOF) return 0;
	add_diag(parser, AST_DIAG_UNEXPECTED_EOF, parser->cur->span);
	return 1;
}

static AstIdent *consume_ident(Parser *parser) {
	assert(parser->cur->kind == TOK_IDENTIFIER && "Unexpected token kind");
	AstIdent *ident = parser_alloc_one(parser, AstIdent);
	ident->span = parser->cur->span;
	parser->cur++;
	return ident;
}

typedef struct {
	uint8_t left;
	uint8_t right;
} BindingPower;

static const uint8_t LOWEST_BP = 0;

static BindingPower get_type_bp(TokenKind kind) {
	switch (kind) {
	case TOK_AMP:
		return (BindingPower){.left = 0, .right = 1};
	case TOK_STAR:
		return (BindingPower){.left = 2, .right = 0};
	case TOK_LBRACKET:
		return (BindingPower){.left = 3, .right = 0};
	default:
		assert(0 && "Unreachable");
	}
}

static Span span_span(Span left, Span right) {
	return (Span){.start = left.start, .len = right.start + right.len - left.start};
}

static AstType *consume_type(Parser *parser, uint8_t ambient_bp) {
	AstType *base_type = NULL;

	const Token *cur_tok;
	while (1) {
		cur_tok = parser->cur;
		if (cur_tok->kind == TOK_LPAREN) {
			assert(base_type == NULL);
			consume(parser, TOK_LPAREN);
			base_type = consume_type(parser, LOWEST_BP);
			consume(parser, TOK_RPAREN);
			continue;
		}
		if (cur_tok->kind == TOK_AMP) {
			assert(base_type == NULL);
			uint8_t bp = get_type_bp(TOK_AMP).right;
			const Token *amp_tok = parser->cur;
			consume(parser, TOK_AMP);
			cur_tok = parser->cur;
			AstTypeKind borrow_kind = AST_TYPE_BORROW;
			if (cur_tok->kind == TOK_KEY_MUT) {
				borrow_kind = AST_TYPE_BORROW_MUT;
				consume(parser, TOK_KEY_MUT);
			}
			AstType *operand = consume_type(parser, bp);
			base_type = parser_alloc_one(parser, AstType);
			base_type->kind = borrow_kind;
			if (borrow_kind == AST_TYPE_BORROW) {
				base_type->borrow.inner = operand;
			} else if (borrow_kind == AST_TYPE_BORROW_MUT) {
				base_type->borrow_mut.inner = operand;
			} else assert(0);
			base_type->span = span_span(amp_tok->span, operand->span);
			continue;
		}
		if (cur_tok->kind == TOK_IDENTIFIER) {
			assert(base_type == NULL);
			AstIdent *ident = consume_ident(parser);
			base_type = parser_alloc_one(parser, AstType);
			base_type->kind = AST_TYPE_NAME;
			base_type->name.ident = ident;
			base_type->span = ident->span;
			continue;
		}
		if (cur_tok->kind == TOK_STAR) {
			assert(base_type != NULL);
			uint8_t bp = get_type_bp(TOK_STAR).left;
			if (bp <= ambient_bp) {
				return base_type;
			}
			consume(parser, TOK_STAR);
			AstType *new_base_type = parser_alloc_one(parser, AstType);
			new_base_type->kind = AST_TYPE_POINTER;
			new_base_type->pointer.inner = base_type;
			new_base_type->span = span_span(base_type->span, cur_tok->span);
			base_type = new_base_type;
			continue;
		}
		if (cur_tok->kind == TOK_LBRACKET) {
			assert(base_type != NULL);
			uint8_t bp = get_type_bp(TOK_LBRACKET).left;
			if (bp <= ambient_bp) {
				return base_type;
			}
			consume(parser, TOK_LBRACKET);
			consume(parser, TOK_RBRACKET); // TODO: array_fixed
			AstType *new_base_type = parser_alloc_one(parser, AstType);
			new_base_type->kind = AST_TYPE_ARRAY_DYN;
			new_base_type->array_dyn.inner = base_type;
			new_base_type->span = span_span(base_type->span, (cur_tok+1)->span);
			base_type = new_base_type;
			continue;
		}
		break;
	}
	return base_type;
}

static AstStructField *consume_struct_field(Parser *parser) {
	AstIdent *field_name = consume_ident(parser);
	consume(parser, TOK_COLON);
	AstType *field_type = consume_type(parser, LOWEST_BP);
	AstStructField *field = parser_alloc_one(parser, AstStructField);
	field->name = field_name;
	field->type = field_type;
	field->span = span_span(field_name->span, field_type->span);
	field->next = NULL;
	return field;
}

static AstStruct *parse_def_struct(Parser *parser) {
	const Token *struct_tok = parser->cur;
	consume(parser, TOK_KEY_STRUCT);

	AstStruct *struct_def = parser_alloc_one(parser, AstStruct);

	struct_def->name = consume_ident(parser);

	consume(parser, TOK_LBRACE);
	AstStructField **cur = &struct_def->fields;
	while (parser->cur->kind != TOK_RBRACE) {
		if (guard_eof(parser)) {
			struct_def->span = span_span(struct_tok->span, parser->cur->span);
			return struct_def;
		}
		AstStructField *field = consume_struct_field(parser);
		consume(parser, TOK_SEMICOLON);
		*cur = field;
		cur = &field->next;
	}
	struct_def->span = span_span(struct_tok->span, parser->cur->span);
	consume(parser, TOK_RBRACE);
	return struct_def;
}

static AstItem *parse_item(Parser *parser) {
	const Token *starting_token = parser->cur;

	AstItem *item = parser_alloc_one(parser, AstItem);

	switch (starting_token->kind) {
	case TOK_KEY_STRUCT:
		item->kind = AST_ITEM_STRUCT;
		item->struc = parse_def_struct(parser);
		break;
	default:
		assert(0 && "Unimplemented item");
	}
	return item;
}

Parser parse(const char *filename, Token *tokens) {
	Parser parser = (Parser){.filename = filename, .cur = tokens};
	arena_init(&parser.arena);
	vec_init  (&parser.tree.items, DEFAULT_AST_ITEMS_CAP);
	vec_init  (&parser.diags, 1);

	while (parser.cur->kind != TOK_EOF) {
		AstItem *item = parse_item(&parser);
		vec_push(&parser.tree.items, &item);
	}

	return parser;
}

static void print_tab(size_t depth) {
	if (depth > 0) printf("%*s", (int)(depth * 4), "");
}

static void print_ident(const char *src, AstIdent *ident) {
	printf(CLR_MAGENTA "%.*s" CLR_END, (int)(ident->span.len), src + ident->span.start);
}

static void print_type(const char *src, AstType *type) {
	switch (type->kind) {
	case AST_TYPE_UNKNOWN:
		printf(CLR_YELLOW "UNKNOWN" CLR_END);
		break;
	case AST_TYPE_NAME:
		print_ident(src, type->name.ident);
		break;
	case AST_TYPE_BORROW:
		printf(CLR_YELLOW "BORROW" CLR_END "(");
		print_type(src, type->borrow.inner);
		printf(")");
		break;
	case AST_TYPE_BORROW_MUT:
		printf(CLR_YELLOW "BORROW_MUT" CLR_END "(");
		print_type(src, type->borrow_mut.inner);
		printf(")");
		break;
	case AST_TYPE_POINTER:
		printf(CLR_YELLOW "POINTER" CLR_END "(");
		print_type(src, type->pointer.inner);
		printf(")");
		break;
	case AST_TYPE_ARRAY_DYN:
		printf(CLR_YELLOW "ARRAY_DYN" CLR_END "(");
		print_type(src, type->array_dyn.inner);
		printf(")");
		break;
	default:
		assert(0);
	}
}

static void print_struct_field(const char *src, AstStructField *field, size_t depth) {
	print_tab(depth);
	printf(CLR_CYAN "|>" CLR_GREEN " FIELD " CLR_END);
	print_ident(src, field->name);
	printf(CLR_GREEN " TYPE " CLR_END);
	print_type(src, field->type);
	printf("\n");
}

static void print_struct(const char *src, AstStruct *struc, size_t depth) {
	print_tab(depth);
	printf(CLR_GREEN "STRUCT " CLR_END);
	print_ident(src, struc->name);
	printf("\n");
	AstStructField *field = struc->fields;
	while (field != NULL) {
		print_struct_field(src, field, depth + 1);
		field = field->next;
	}
}

static void print_item(const char *src, AstItem *item, size_t depth) {
	switch (item->kind) {
	case AST_ITEM_FUNCTION:
		assert(0);
	case AST_ITEM_STRUCT:
		print_struct(src, item->struc, depth);
		break;
	}
}

void print_ast(const char *src, Ast *ast) {
	for (size_t i = 0; i < ast->items.len; ++i) {
		print_item(src, ast->items.data[i], 0);
	}
}
