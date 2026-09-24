#ifndef PARSE_EXPR_H
#define PARSE_EXPR_H

#include <smew/ast.h>
#include <smew/parse.h>

#include <stdint.h>

AstExpr *unit_expr   (Parser *parser);
AstExpr *unknown_expr(Parser *parser);

AstExpr *parse_expr        (Parser *parser, uint8_t min_bp);
int      parse_and_sequence(Parser *parser, AstExpr **base);

#endif
