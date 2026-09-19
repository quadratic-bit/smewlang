#include <assert.h>
#include <smew/ast.h>

#include <smew/colors.h>

enum {
	CONTINUATION_LIMIT = 128
};

static const char *binary_op_str(AstOpKindBinary kind) {
	switch (kind) {
	case AST_OP_BINARY_SEQ:      return ";";
	case AST_OP_BINARY_ACCESSOR: return ".";
	case AST_OP_BINARY_ASSIGN:   return "=";
	case AST_OP_BINARY_PLUS:     return "+";
	case AST_OP_BINARY_MULT:     return "*";
	case AST_OP_BINARY_DIV:      return "/";
	case AST_OP_BINARY_MINUS:    return "-";
	case AST_OP_BINARY_EQ:       return "==";
	case AST_OP_BINARY_NEQ:      return "!=";
	case AST_OP_BINARY_GE:       return ">=";
	case AST_OP_BINARY_GT:       return ">";
	case AST_OP_BINARY_LE:       return "<=";
	case AST_OP_BINARY_LT:       return "<";
	}
}

static const char *unary_op_str(AstOpKindUnary kind) {
	switch (kind) {
	case AST_OP_UNARY_NOT:    return "!";
	case AST_OP_UNARY_UNWRAP: return "?";
	case AST_OP_UNARY_BORROW: return "&";
	case AST_OP_UNARY_DEREF:  return "*";
	case AST_OP_UNARY_MOVE:   return "move";
	}
}

static void print_tab(size_t depth, int cont[]) {
	assert(depth < CONTINUATION_LIMIT && "Depth exceeded the limit");

	if (depth == 0) return;

	for (size_t i = 0; i + 1 < depth; ++i) {
		if (cont[i]) printf(CLR_BLUE "|" CLR_END "  ");
		else         printf("   ");
	}

	printf(CLR_BLUE "|--" CLR_END);
}

static void print_ident(const char *src, const AstIdent *ident) {
	if (ident->span.len == 0) {
		printf(CLR_RED "<UNK>" CLR_END);
	} else {
		printf(CLR_MAGENTA "%.*s" CLR_END, (int)(ident->span.len), src + ident->span.start);
	}
}

static void print_literal(const char *src, const AstLiteral *lit) {
	switch (lit->kind) {
	case AST_LITERAL_INT:
		printf(CLR_YELLOW "int(%.*s)" CLR_END, (int)(lit->span.len), src + lit->span.start);
		break;
	case AST_LITERAL_UNIT:
		printf(CLR_YELLOW "unit" CLR_END);
		break;
	default:
		// TODO:
		assert(0 && "Print for this literal type is not implemented");
	}
}

static void print_expr(const char *src, const AstExpr *expr, size_t depth, int cont[]) {
	switch (expr->kind) {
	case AST_EXPR_UNKNOWN:
		print_tab(depth, cont);
		printf(CLR_RED "<UNK>" CLR_END);
		printf("\n");
		break;

	case AST_EXPR_IDENT:
		print_tab(depth, cont);
		printf(CLR_GREEN "IDENTIFIER " CLR_END);
		print_ident(src, expr->ident);
		printf("\n");
		break;

	case AST_EXPR_LITERAL:
		print_tab(depth, cont);
		printf(CLR_GREEN "LITERAL " CLR_END);
		print_literal(src, expr->literal);
		printf("\n");
		break;

	case AST_EXPR_OP_BINARY:
		if (expr->op_binary->op == AST_OP_BINARY_SEQ) {
			int sequence_has_next = cont[depth - 1];

			cont[depth - 1] = 1;
			print_expr(src, expr->op_binary->left,  depth, cont);

			cont[depth - 1] = sequence_has_next;
			print_expr(src, expr->op_binary->right, depth, cont);

			break;
		}
		print_tab(depth, cont);

		printf(CLR_GREEN "BINARY " CLR_YELLOW "%s" CLR_END "\n",
		       binary_op_str(expr->op_binary->op));

		cont[depth] = 1;
		print_expr(src, expr->op_binary->left, depth + 1, cont);

		cont[depth] = 0;
		print_expr(src, expr->op_binary->right, depth + 1, cont);
		break;

	case AST_EXPR_OP_UNARY:
		print_tab(depth, cont);
		printf(CLR_GREEN "UNARY EXPR " CLR_YELLOW "%s" CLR_END "\n",
		       unary_op_str(expr->op_unary->op));

		print_expr(src, expr->op_unary->operand, depth + 1, cont);
		break;

	default:
		// TODO:
		assert(0 && "Print for this expression type is not implemented");
	}
}

static void print_type(const char *src, const AstType *type, size_t depth, int cont[]) {
	print_tab(depth, cont);
	switch (type->kind) {
	case AST_TYPE_UNKNOWN:
		printf(CLR_YELLOW "UNKNOWN" CLR_END "\n");
		break;
	case AST_TYPE_NAME:
		print_ident(src, type->name.ident);
		printf("\n");
		break;
	case AST_TYPE_BORROW:
		printf(CLR_YELLOW "BORROW" CLR_END "\n");
		print_type(src, type->borrow.inner, depth + 1, cont);
		break;
	case AST_TYPE_BORROW_MUT:
		printf(CLR_YELLOW "BORROW_MUT" CLR_END "\n");
		print_type(src, type->borrow_mut.inner, depth + 1, cont);
		break;
	case AST_TYPE_POINTER:
		printf(CLR_YELLOW "POINTER" CLR_END "\n");
		print_type(src, type->pointer.inner, depth + 1, cont);
		break;
	case AST_TYPE_ARRAY_DYN:
		printf(CLR_YELLOW "ARRAY_DYN" CLR_END "\n");
		print_type(src, type->array_dyn.inner, depth + 1, cont);
		break;
	case AST_TYPE_ARRAY_FIXED:
		printf(CLR_YELLOW "ARRAY_FIXED" CLR_END "\n");
		print_tab(depth + 1, cont);
		printf(CLR_GREEN "TYPE" CLR_END "\n");
		cont[depth] = 1;
		print_type(src, type->array_fixed.inner, depth + 2, cont);
		cont[depth] = 0;
		print_tab(depth + 1, cont);
		printf(CLR_GREEN "LENGTH" CLR_END "\n");
		print_expr(src, type->array_fixed.length, depth + 2, cont);
		break;
	default:
		// TODO:
		assert(0 && "Print for this type is not implemented");
	}
}

static void print_struct_field(const char *src, const AstStructField *field, size_t depth, int cont[]) {
	print_tab(depth, cont);
	printf(CLR_GREEN " FIELD" CLR_END "\n");
	print_tab(depth + 1, cont);
	printf(CLR_GREEN "NAME " CLR_END);
	print_ident(src, field->name);
	printf("\n");
	print_tab(depth + 1, cont);
	printf(CLR_GREEN "TYPE" CLR_END);
	printf("\n");
	print_type(src, field->type, depth + 2, cont);
}

static void print_struct(const char *src, const AstStruct *struc, size_t depth, int cont[]) {
	print_tab(depth, cont);
	printf(CLR_GREEN "STRUCT " CLR_END);
	print_ident(src, struc->name);
	printf("\n");
	cont[depth] = 1;
	AstStructField *field = struc->fields;
	while (field != NULL) {
		if (field->next == NULL) {
			cont[depth] = 0;
		}
		print_struct_field(src, field, depth + 1, cont);
		field = field->next;
	}
}

static void print_func_param(const char *src, const AstFunctionParam *param, size_t depth, int cont[]) {
	print_tab(depth, cont);
	printf(CLR_GREEN "PARAM" CLR_END "\n");
	print_tab(depth + 1, cont);
	printf(CLR_GREEN "NAME " CLR_END);
	print_ident(src, param->name);
	printf("\n");
	print_tab(depth + 1, cont);
	printf(CLR_GREEN "TYPE" CLR_END "\n");
	print_type(src, param->type, depth + 2, cont);
}

static void print_func(const char *src, const AstFunction *func, size_t depth, int cont[]) {
	print_tab(depth, cont);
	if (func->is_public) {
		printf(CLR_CYAN "PUB " CLR_END);
	}
	printf(CLR_GREEN "FUNCTION" CLR_END "\n");
	cont[depth] = 1;
	print_tab(depth + 1, cont);
	printf(CLR_GREEN "NAME " CLR_END);
	print_ident(src, func->name);
	printf("\n");
	print_tab(depth + 1, cont);
	printf(CLR_GREEN "RETURNS" CLR_END "\n");
	print_type(src, func->return_type, depth + 2, cont);
	AstFunctionParam *param = func->params;
	while (param != NULL) {
		print_func_param(src, param, depth + 1, cont);
		param = param->next;
	}
	cont[depth] = 0;
	print_tab(depth + 1, cont);
	printf(CLR_GREEN "BLOCK" CLR_END "\n");
	print_expr(src, func->block->body, depth + 2, cont);
}

static void print_item(const char *src, const AstItem *item, size_t depth, int cont[]) {
	switch (item->kind) {
	case AST_ITEM_FUNCTION:
		print_func(src, item->function, depth, cont);
		break;
	case AST_ITEM_STRUCT:
		print_struct(src, item->struc, depth, cont);
		break;
	}
}

void print_ast(const char *src, const Ast *ast) {
	int cont[CONTINUATION_LIMIT] = {0};
	for (size_t i = 0; i < ast->items.len; ++i) {
		print_item(src, ast->items.data[i], 0, cont);
		printf("\n");
	}
}
