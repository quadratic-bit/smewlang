#include "smew/vec.h"
#include <assert.h>
#include <stdio.h>

#include <smew/buf.h>
#include <smew/lex.h>
#include <smew/colors.h>
#include <smew/parse.h>

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

	Lexer lexer = lex(&source, input_filename);

	for (size_t i = 0; i < lexer.toks.len; ++i) {
		Token tok = lexer.toks.data[i];
		printf(CLR_GREEN "%s" CLR_END, token_kind_name(tok.kind));
		if (tok.kind == TOK_IDENTIFIER  ||
		    tok.kind == TOK_LITERAL_INT ||
		    tok.kind == TOK_LITERAL_STRING)
		{
			printf("(" CLR_MAGENTA "%.*s" CLR_END ")",
			       (int)tok.span.len, lexer.src->data + tok.span.start);
		}
		putchar('\n');
	}

	printf("\n");

	for (size_t i = 0; i < lexer.diags.len; ++i) {
		LexDiag *diag = &lexer.diags.data[i];
		print_diag(&lexer, diag);
		printf("\n");
	}
	if (lexer.diags.len != 0) {
		lex_free(&lexer);
		vec_free(&source);
		fclose(input_file);
		return 1;
	}

	Parser parser = parse(input_filename, lexer.toks.data);
	print_ast(lexer.src->data, &parser.tree);

	vec_free  (&parser.tree.items);
	arena_free(&parser.arena);

	lex_free(&lexer);
	vec_free(&source);
	fclose(input_file);
	return 0;
}
