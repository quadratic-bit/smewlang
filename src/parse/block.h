#ifndef PARSE_BLOCK_H
#define PARSE_BLOCK_H

#include <smew/ast.h>
#include <smew/parse.h>

AstBlock *parse_block(Parser *parser);
AstBlock *empty_block(Parser *parser);

#endif
