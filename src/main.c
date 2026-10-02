#include <smew/arena.h>
#include <smew/diag.h>
#include <smew/ast.h>
#include <smew/lex.h>
#include <smew/parse.h>
#include <smew/source.h>
#include <smew/vec.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef enum {
	MODE_LEX,
	MODE_PARSE,
} ParsingMode;

int main(int argc, char **argv) {
	if (argc != 3) {
		if (argc == 0 || argv[0] == NULL)
			puts("usage: lex <lex|parse> <filename>");
		else
			printf("usage: %s <lex|parse> <filename>\n", argv[0]);

		return 1;
	}

	ParsingMode mode;

	if (!strcmp(argv[1], "lex")) {
		mode = MODE_LEX;
	} else if (!strcmp(argv[1], "parse")) {
		mode = MODE_PARSE;
	} else {
		printf("Unrecognized parsing mode: %s\n", argv[1]);
		return 1;
	}

	const char *input_filename = argv[2];
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

	for (size_t i = 0; i < lexer.diags.len; ++i) {
		Diag *diag = &lexer.diags.data[i];
		print_diag(&source, diag);
		printf("\n");
	}

	if (mode == MODE_LEX) {
		print_tokens(&lexer);
		lex_free(&lexer);
		vec_free(&source.buf);
		fclose(input_file);
		return 0;
	}

	if (lexer.diags.len != 0) {
		lex_free(&lexer);
		vec_free(&source.buf);
		fclose(input_file);
		return 1;
	}

	Parser parser = parse(&source, lexer.toks.data);
	print_ast(lexer.src->buf.data, &parser.tree);

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
