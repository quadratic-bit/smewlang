#include <assert.h>
#include <smew/ast.h>

#include <smew/colors.h>

static const char *binary_op_str(AstOpKindBinary kind) {
	switch (kind) {
	case AST_OP_BINARY_SEQ:      return "sequence";
	case AST_OP_BINARY_ACCESSOR: return "aceess";
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

static void print_tab(size_t depth) {
	if (depth > 0) printf("%*s", (int)(depth * 4), "");
}

static void print_ident(const char *src, AstIdent *ident) {
	if (ident->span.len == 0) {
		printf(CLR_RED "<UNK>" CLR_END);
	} else {
		printf(CLR_MAGENTA "%.*s" CLR_END, (int)(ident->span.len), src + ident->span.start);
	}
}

static void print_type(const char *src, AstType *type) {
	switch (type->kind) {
	case AST_TYPE_UNKNOWN:
		printf(CLR_YELLOW "UNKNOWN" CLR_END);
		break;
	case AST_TYPE_NAME:
		print_ident(src, type->name.ident);
		break;
	case AST_TYPE_BORROW:
		printf(CLR_YELLOW "BORROW" CLR_END "(");
		print_type(src, type->borrow.inner);
		printf(")");
		break;
	case AST_TYPE_BORROW_MUT:
		printf(CLR_YELLOW "BORROW_MUT" CLR_END "(");
		print_type(src, type->borrow_mut.inner);
		printf(")");
		break;
	case AST_TYPE_POINTER:
		printf(CLR_YELLOW "POINTER" CLR_END "(");
		print_type(src, type->pointer.inner);
		printf(")");
		break;
	case AST_TYPE_ARRAY_DYN:
		printf(CLR_YELLOW "ARRAY_DYN" CLR_END "(");
		print_type(src, type->array_dyn.inner);
		printf(")");
		break;
	default:
		// TODO:
		assert(0 && "Print for this type is not implemented");
	}
}

static void print_struct_field(const char *src, AstStructField *field, size_t depth) {
	print_tab(depth);
	printf(CLR_CYAN "|>" CLR_GREEN " FIELD " CLR_END);
	print_ident(src, field->name);
	printf(CLR_GREEN " TYPE " CLR_END);
	print_type(src, field->type);
	printf("\n");
}

static void print_struct(const char *src, AstStruct *struc, size_t depth) {
	print_tab(depth);
	printf(CLR_GREEN "STRUCT " CLR_END);
	print_ident(src, struc->name);
	printf("\n");
	AstStructField *field = struc->fields;
	while (field != NULL) {
		print_struct_field(src, field, depth + 1);
		field = field->next;
	}
}

static void print_func_param(const char *src, AstFunctionParam *param, size_t depth) {
	print_tab(depth);
	printf(CLR_CYAN "|>" CLR_GREEN " PARAM " CLR_END);
	print_ident(src, param->name);
	printf(CLR_GREEN " TYPE " CLR_END);
	print_type(src, param->type);
	printf("\n");
}

static void print_expr(const char *src, AstExpr *expr, size_t depth) {
	switch (expr->kind) {
	case AST_EXPR_OP_BINARY:
		print_tab(depth);
		printf(CLR_GREEN "BINARY EXPR " CLR_YELLOW "'%s'" CLR_END "\n",
		       binary_op_str(expr->op_binary->op));

		print_tab(depth);
		printf(CLR_BLUE "LHS" CLR_END "\n");

		print_expr(src, expr->op_binary->left, depth + 1);

		print_tab(depth);
		printf(CLR_BLUE "RHS" CLR_END "\n");

		print_expr(src, expr->op_binary->right, depth + 1);
		break;
	
	case AST_EXPR_OP_UNARY:
		print_tab(depth);
		printf(CLR_GREEN "UNARY EXPR" CLR_END "\n");
		print_expr(src, expr->op_unary->operand, depth + 1);
		break;

	case AST_EXPR_IDENT:
		print_tab(depth);
		printf(CLR_GREEN "IDENTIFIER " CLR_END);
		print_ident(src, expr->ident);
		printf("\n");
		break;
	default:
		// TODO:
		assert(0 && "Print for this expression type is not implemented");
	}
}

static void print_func(const char *src, AstFunction *func, size_t depth) {
	print_tab(depth);
	if (func->is_public) {
		printf(CLR_CYAN "PUB " CLR_END);
	}
	printf(CLR_GREEN "FUNCTION " CLR_END);
	print_ident(src, func->name);
	printf("\n");
	AstFunctionParam *param = func->params;
	while (param != NULL) {
		print_func_param(src, param, depth + 1);
		param = param->next;
	}
	print_tab(depth);
	printf(CLR_GREEN "BLOCK" CLR_END "\n");
	print_expr(src, func->block->body, depth + 1);
}

static void print_item(const char *src, AstItem *item, size_t depth) {
	switch (item->kind) {
	case AST_ITEM_FUNCTION:
		print_func(src, item->function, depth);
		break;
	case AST_ITEM_STRUCT:
		print_struct(src, item->struc, depth);
		break;
	}
}

void print_ast(const char *src, Ast *ast) {
	for (size_t i = 0; i < ast->items.len; ++i) {
		print_item(src, ast->items.data[i], 0);
	}
}
