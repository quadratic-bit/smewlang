#ifndef PARSE_FUNC
#define PARSE_FUNC

#include <smew/parse.h>
#include <smew/ast.h>

AstFunctionDeclaration *parse_func_decl         (Parser *parser);
void                    parse_func_def_with_decl(Parser *parser, AstFunction *func_def);
AstFunction            *parse_func_def          (Parser *parser);

AstGenericList *parse_generic_list(Parser *parser);

#endif
