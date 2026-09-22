#ifndef PARSE_TYPE_H
#define PARSE_TYPE_H

#include <smew/ast.h>
#include <smew/parse.h>

#include <stdint.h>

AstType *unknown_type(Parser *parser);
AstType *parse_type  (Parser *parser, uint8_t min_bp);

#endif
