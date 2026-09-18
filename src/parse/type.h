#ifndef PARSE_TYPE_H
#define PARSE_TYPE_H

#include <smew/parse.h>

AstType *unknown_type(Parser *parser);
AstType *parse_type  (Parser *parser, uint8_t ambient_bp);

#endif
