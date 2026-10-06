#ifndef PARSE_FUNC
#define PARSE_FUNC

#include <smew/parse.h>
#include <smew/ast.h>

AstFunctionDeclaration *parse_func_decl(Parser *parser);
AstFunction            *parse_func_def (Parser *parser);

#endif
