#include <smew/arena.h>
#include <smew/ast.h>
#include <smew/colors.h>
#include <smew/lex.h>
#include <smew/parse.h>
#include <smew/source.h>
#include <smew/vec.h>

#include <assert.h>
#include <stdio.h>

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

	SourceFile source;
	vec_init(&source.buf, BUFSIZ);
	source.filename = input_filename;

	if (!source.buf.data) return 1;
	assert(source.buf.cap > 0);
	assert(source.buf.len == 0);

	if (file_read(&source, input_file) == BUF_ERR) {
		vec_free(&source.buf);
		fclose(input_file);
		return 1;
	}

	Lexer lexer = lex(&source);

	for (size_t i = 0; i < lexer.toks.len; ++i) {
		Token tok = lexer.toks.data[i];
		printf(CLR_GREEN "%s" CLR_END, token_kind_name(tok.kind));
		if (tok.kind == TOK_IDENTIFIER  ||
		    tok.kind == TOK_LITERAL_INT ||
		    tok.kind == TOK_LITERAL_STRING)
		{
			printf("(" CLR_MAGENTA "%.*s" CLR_END ")",
			       (int)tok.span.len, lexer.src->buf.data + tok.span.start);
		}
		putchar('\n');
	}

	putchar('\n');

	for (size_t i = 0; i < lexer.diags.len; ++i) {
		Diag *diag = &lexer.diags.data[i];
		print_diag(&source, diag);
		printf("\n");
	}
	if (lexer.diags.len != 0) {
		lex_free(&lexer);
		vec_free(&source.buf);
		fclose(input_file);
		return 1;
	}

	Parser parser = parse(lexer.src, lexer.toks.data);
	print_ast(lexer.src->buf.data, &parser.tree);

	putchar('\n');

	for (size_t i = 0; i < parser.diags.len; ++i) {
		Diag *diag = &parser.diags.data[i];
		print_diag(lexer.src, diag);
		printf("\n");
	}

	vec_free  (&parser.tree.items);
	vec_free  (&parser.diags);
	arena_free(&parser.arena);

	lex_free(&lexer);
	vec_free(&source.buf);
	fclose(input_file);
	return 0;
}
