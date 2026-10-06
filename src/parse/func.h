#ifndef PARSE_FUNC
#define PARSE_FUNC

#include <smew/parse.h>
#include <smew/ast.h>

AstFunction *parse_decl_func(Parser *parser);
AstFunction *parse_def_func (Parser *parser);

#endif
