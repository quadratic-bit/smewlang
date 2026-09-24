#include <smew/ast.h>

#include <smew/colors.h>

#include <assert.h>
#include <stdio.h>

enum {
	PRINT_MAX_DEPTH = 128
};

typedef struct {
	const char *src;
	int has_next_sibling[PRINT_MAX_DEPTH];
} AstPrinter;

typedef struct {
	AstPrinter *printer;
	size_t      depth;
} PrintCtx;

static PrintCtx deep(PrintCtx old, short depth_delta) {
	if (depth_delta < 0) {
		size_t abs_delta = (size_t)(-depth_delta);
		assert(old.depth >= abs_delta && "Negative depth");
		return (PrintCtx){.printer = old.printer, .depth = old.depth - abs_delta};
	}
	// no assert, since I assume CONTINUATION_LIMIT << SIZE_MAX, so no overflow possible
	return (PrintCtx){.printer = old.printer, .depth = old.depth + (size_t)depth_delta};
}

static void set_next_sibling(PrintCtx ctx, int has_next) {
	assert(ctx.depth < PRINT_MAX_DEPTH && "Depth exceeded the limit");
	ctx.printer->has_next_sibling[ctx.depth] = has_next;
}

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
	case AST_OP_UNARY_RETURN: return "return";
	}
}

static void print_tab(PrintCtx ctx) {
	assert(ctx.depth < PRINT_MAX_DEPTH && "Depth exceeded the limit");

	if (ctx.depth == 0) return;

	for (size_t d = 0; d + 1 < ctx.depth; ++d) {
		if (ctx.printer->has_next_sibling[d]) printf(CLR_BLUE "|" CLR_END "  ");
		else                                  printf("   ");
	}

	printf(CLR_BLUE "|--" CLR_END);
}

static void print_ident(PrintCtx ctx, const AstIdent *ident) {
	if (ident->span.len == 0) {
		printf(CLR_RED "<UNK>" CLR_END);
	} else {
		printf(CLR_MAGENTA "%.*s" CLR_END, (int)(ident->span.len),
		       ctx.printer->src + ident->span.start);
	}
}

static void print_literal(PrintCtx ctx, const AstLiteral *lit) {
	switch (lit->kind) {
	case AST_LITERAL_INT:
		printf(CLR_YELLOW "int(%.*s)" CLR_END, (int)(lit->span.len),
		       ctx.printer->src + lit->span.start);
		break;
	case AST_LITERAL_UNIT:
		printf(CLR_YELLOW "unit" CLR_END);
		break;
	default:
		// TODO:
		assert(0 && "Print for this literal type is not implemented");
	}
}

static void print_expr(PrintCtx ctx, const AstExpr *expr) {
	switch (expr->kind) {
	case AST_EXPR_UNKNOWN:
		print_tab(ctx);
		printf(CLR_RED "<UNK>" CLR_END "\n");
		break;

	case AST_EXPR_IDENT:
		print_tab(ctx);
		printf(CLR_GREEN "IDENTIFIER " CLR_END);
		print_ident(ctx, expr->ident);
		putchar('\n');
		break;

	case AST_EXPR_LITERAL:
		print_tab(ctx);
		printf(CLR_GREEN "LITERAL " CLR_END);
		print_literal(ctx, expr->literal);
		putchar('\n');
		break;

	case AST_EXPR_OP_BINARY:
		if (expr->op_binary->op == AST_OP_BINARY_SEQ) {
			int seq_has_next = ctx.printer->has_next_sibling[ctx.depth - 1];

			set_next_sibling(deep(ctx, -1), 1);
			print_expr(ctx, expr->op_binary->left);

			set_next_sibling(deep(ctx, -1), seq_has_next);
			print_expr(ctx, expr->op_binary->right);

			break;
		}
		print_tab(ctx);

		printf(CLR_GREEN "BINARY " CLR_YELLOW "%s" CLR_END "\n",
		       binary_op_str(expr->op_binary->op));

		set_next_sibling(ctx, 1);
		print_expr(deep(ctx, +1), expr->op_binary->left);

		set_next_sibling(ctx, 0);
		print_expr(deep(ctx, +1), expr->op_binary->right);
		break;

	case AST_EXPR_OP_UNARY:
		print_tab(ctx);
		printf(CLR_GREEN "UNARY EXPR " CLR_YELLOW "%s" CLR_END "\n",
		       unary_op_str(expr->op_unary->op));

		print_expr(deep(ctx, +1), expr->op_unary->operand);
		break;

	case AST_EXPR_LOOP:
		print_tab(ctx);
		printf(CLR_GREEN "LOOP BLOCK" CLR_END "\n");

		print_expr(deep(ctx, +1), expr->loop->body->body);
		break;

	case AST_EXPR_BREAK:
		print_tab(ctx);
		printf(CLR_GREEN "BREAK" CLR_END "\n");

		break;

	case AST_EXPR_IF:
		print_tab(ctx);
		printf(CLR_GREEN "BRANCH" CLR_END "\n");

		AstCondBlock *cur = expr->branch->conds;
		set_next_sibling(ctx, 1);

		while (cur != NULL) {
			print_tab(deep(ctx, +1));
			printf(CLR_GREEN "COND IF" CLR_END "\n");
			print_expr(deep(ctx, +2), cur->cond);

			if (cur->next == NULL) {
				set_next_sibling(ctx, 0);
			}

			print_tab(deep(ctx, +1));
			printf(CLR_GREEN "BLOCK" CLR_END "\n");
			print_expr(deep(ctx, +2), cur->block->body);
			cur = cur->next;
		}
		break;

	default:
		// TODO:
		assert(0 && "Print for this expression type is not implemented");
	}
}

static void print_type(PrintCtx ctx, const AstType *type) {
	print_tab(ctx);
	switch (type->kind) {
	case AST_TYPE_UNKNOWN:
		printf(CLR_YELLOW "UNKNOWN" CLR_END "\n");
		break;

	case AST_TYPE_NAME:
		print_ident(ctx, type->name.ident);
		putchar('\n');
		break;

	case AST_TYPE_BORROW:
		printf(CLR_YELLOW "BORROW" CLR_END "\n");
		print_type(deep(ctx, +1), type->borrow.inner);
		break;

	case AST_TYPE_BORROW_MUT:
		printf(CLR_YELLOW "BORROW_MUT" CLR_END "\n");
		print_type(deep(ctx, +1), type->borrow_mut.inner);
		break;

	case AST_TYPE_POINTER:
		printf(CLR_YELLOW "POINTER" CLR_END "\n");
		print_type(deep(ctx, +1), type->pointer.inner);
		break;

	case AST_TYPE_ARRAY_DYN:
		printf(CLR_YELLOW "ARRAY_DYN" CLR_END "\n");
		print_type(deep(ctx, +1), type->array_dyn.inner);
		break;

	case AST_TYPE_ARRAY_FIXED:
		printf(CLR_YELLOW "ARRAY_FIXED" CLR_END "\n");

		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "TYPE" CLR_END "\n");

		set_next_sibling(ctx, 1);
		print_type(deep(ctx, +2), type->array_fixed.inner);
		set_next_sibling(ctx, 0);

		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "LENGTH" CLR_END "\n");
		print_expr(deep(ctx, +2), type->array_fixed.length);
		break;

	case AST_TYPE_GENERIC:
		printf(CLR_YELLOW "GENERIC" CLR_END "\n");

		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "TYPE" CLR_END "\n");

		set_next_sibling(ctx, 1);
		print_type(deep(ctx, +2), type->generic.base);
		set_next_sibling(ctx, 0);

		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "ARGS" CLR_END "\n");

		set_next_sibling(deep(ctx, +1), 1);

		AstTypeGeneric *cur = type->generic.args;
		while (cur != NULL) {
			if (cur->next == NULL) {
				set_next_sibling(deep(ctx, +1), 0);
			}
			print_type(deep(ctx, +2), cur->arg);
			cur = cur->next;
		}

		set_next_sibling(deep(ctx, +1), 0);
		break;
	}
}

static void print_struct_field(PrintCtx ctx, const AstStructField *field) {
	print_tab(ctx);
	printf(CLR_GREEN " FIELD" CLR_END "\n");

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "NAME " CLR_END);
	print_ident(ctx, field->name);
	putchar('\n');

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "TYPE" CLR_END "\n");
	print_type(deep(ctx, +2), field->type);
}

static void print_struct(PrintCtx ctx, const AstStruct *struc) {
	print_tab(ctx);
	printf(CLR_GREEN "STRUCT " CLR_END);
	print_ident(ctx, struc->name);
	putchar('\n');

	set_next_sibling(ctx, 1);

	AstStructField *field = struc->fields;
	while (field != NULL) {
		if (field->next == NULL) {
			set_next_sibling(ctx, 0);
		}
		print_struct_field(deep(ctx, +1), field);
		field = field->next;
	}

	set_next_sibling(ctx, 0);
}

static void print_func_param(PrintCtx ctx, const AstFunctionParam *param) {
	print_tab(ctx);
	printf(CLR_GREEN "PARAM" CLR_END "\n");

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "NAME " CLR_END);
	print_ident(ctx, param->name);
	putchar('\n');

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "TYPE" CLR_END "\n");
	print_type(deep(ctx, +2), param->type);
}

static void print_func(PrintCtx ctx, const AstFunction *func) {
	print_tab(ctx);
	if (func->is_public) {
		printf(CLR_CYAN "PUB " CLR_END);
	}
	printf(CLR_GREEN "FUNCTION" CLR_END "\n");

	set_next_sibling(ctx, 1);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "NAME " CLR_END);
	print_ident(ctx, func->name);
	putchar('\n');

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "RETURNS" CLR_END "\n");
	print_type(deep(ctx, +2), func->return_type);
	AstFunctionParam *param = func->params;
	while (param != NULL) {
		print_func_param(deep(ctx, +1), param);
		param = param->next;
	}

	set_next_sibling(ctx, 0);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "BLOCK" CLR_END "\n");
	print_expr(deep(ctx, +2), func->block->body);
}

static void print_item(PrintCtx ctx, const AstItem *item) {
	switch (item->kind) {
	case AST_ITEM_FUNCTION:
		print_func(ctx, item->function);
		break;
	case AST_ITEM_STRUCT:
		print_struct(ctx, item->struc);
		break;
	}
}

void print_ast(const char *src, const Ast *ast) {
	AstPrinter printer = {.has_next_sibling = {0},      .src   = src};
	PrintCtx   ctx     = {.printer          = &printer, .depth = 0  };

	for (size_t i = 0; i < ast->items.len; ++i) {
		print_item(ctx, ast->items.data[i]);
		printf("\n");
	}
}
