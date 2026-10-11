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
	case AST_OP_BINARY_SEQ:          return ";";
	case AST_OP_BINARY_ACCESSOR:     return ".";
	case AST_OP_BINARY_ASSIGN:       return "=";
	case AST_OP_BINARY_PLUS:         return "+";
	case AST_OP_BINARY_PLUS_ASSIGN:  return "+=";
	case AST_OP_BINARY_MULT:         return "*";
	case AST_OP_BINARY_DIV:          return "/";
	case AST_OP_BINARY_BIN_OR:       return "|";
	case AST_OP_BINARY_BIN_XOR:      return "^";
	case AST_OP_BINARY_BIN_AND:      return "&";
	case AST_OP_BINARY_MINUS:        return "-";
	case AST_OP_BINARY_MINUS_ASSIGN: return "-=";
	case AST_OP_BINARY_EQ:           return "==";
	case AST_OP_BINARY_NEQ:          return "!=";
	case AST_OP_BINARY_AND:          return "&&";
	case AST_OP_BINARY_OR:           return "||";
	case AST_OP_BINARY_GE:           return ">=";
	case AST_OP_BINARY_GT:           return ">";
	case AST_OP_BINARY_LE:           return "<=";
	case AST_OP_BINARY_LT:           return "<";
	}
}

static const char *unary_op_str(AstOpKindUnary kind) {
	switch (kind) {
	case AST_OP_UNARY_NOT:        return "!";
	case AST_OP_UNARY_MINUS:      return "-";
	case AST_OP_UNARY_UNWRAP:     return "?";
	case AST_OP_UNARY_BORROW:     return "&";
	case AST_OP_UNARY_BORROW_MUT: return "&mut";
	case AST_OP_UNARY_DEREF:      return "*";
	case AST_OP_UNARY_MOVE:       return "move";
	case AST_OP_UNARY_RETURN:     return "return";
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

static void print_ident_prefix(PrintCtx ctx, const char *prefix, AstIdent *ident) {
	print_tab(ctx);
	printf("%s", prefix);
	print_ident(ctx, ident);
	putchar('\n');
}

static void print_expr(PrintCtx, const AstExpr *);
static void print_type(PrintCtx, const AstType *);

static void print_literal(PrintCtx ctx, const AstLiteral *lit) {
	print_tab(ctx);
	printf(CLR_GREEN "LITERAL " CLR_END);

	switch (lit->kind) {
	case AST_LITERAL_INT:
		printf(CLR_YELLOW "int(%.*s)\n" CLR_END, (int)(lit->span.len),
		       ctx.printer->src + lit->span.start);
		break;
	case AST_LITERAL_STRING:
		printf(CLR_YELLOW "str(%.*s)\n" CLR_END, (int)(lit->span.len),
		       ctx.printer->src + lit->span.start);
		break;
	case AST_LITERAL_UNIT:
		printf(CLR_YELLOW "unit\n" CLR_END);
		break;
	case AST_LITERAL_BOOL:
		switch (lit->bool) {
		case AST_TRUE:  printf(CLR_YELLOW "true\n"  CLR_END); break;
		case AST_FALSE: printf(CLR_YELLOW "false\n" CLR_END); break;
		}
		break;
	case AST_LITERAL_STRUCT:
		printf(CLR_GREEN "STRUCT " CLR_END);
		print_ident(ctx, lit->struc->type);
		putchar('\n');

		if (lit->struc->fields == NULL) {
			print_tab(deep(ctx, +1));
			printf(CLR_GREEN "NO FIELDS" CLR_END "\n");
			return;
		}

		AstLiteralStructField *cur = lit->struc->fields;
		set_next_sibling(ctx, 1);

		while (cur != NULL) {
			if (cur->next == NULL) {
				set_next_sibling(ctx, 0);
			}

			print_tab(deep(ctx, +1));
			printf(CLR_GREEN "FIELD" CLR_END "\n");

			print_ident_prefix(deep(ctx, +2), CLR_GREEN "NAME ", cur->field);

			print_tab(deep(ctx, +2));
			printf(CLR_GREEN "VALUE" CLR_END "\n");

			print_expr(deep(ctx, +3), cur->value);
			cur = cur->next;
		}
		set_next_sibling(ctx, 0);
		break;
	}
}

static void print_bind(PrintCtx ctx, const AstBind *bind) {
	print_tab(ctx);
	printf(CLR_GREEN "BIND" CLR_END);

	if (bind->mut) {
		printf(CLR_GREEN " MUT" CLR_END);
	}

	putchar('\n');

	print_ident_prefix(deep(ctx, +1), CLR_GREEN "NAME ", bind->name);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "TYPE" CLR_END "\n");

	set_next_sibling(ctx, 1);

	print_type(deep(ctx, +2), bind->type);

	set_next_sibling(ctx, 0);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "VALUE" CLR_END "\n");

	print_expr(deep(ctx, +2), bind->value);
}

static void print_with(PrintCtx ctx, const AstWith *with) {
	print_tab(ctx);
	printf(CLR_GREEN "WITH" CLR_END "\n");

	set_next_sibling(ctx, 1);
	print_bind(deep(ctx, +1), with->bind);
	set_next_sibling(ctx, 0);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "BLOCK" CLR_END "\n");

	print_expr(deep(ctx, +2), with->body->body);

}

static void print_call(PrintCtx ctx, const AstCall *call) {
	print_tab(ctx);
	printf(CLR_GREEN "CALL" CLR_END "\n");

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "CALLEE" CLR_END "\n");

	set_next_sibling(ctx, 1);

	print_expr(deep(ctx, +2), call->callee);

	set_next_sibling(ctx, 0);

	print_tab(deep(ctx, +1));

	if (call->args->arg == NULL) {
		printf(CLR_GREEN "NO ARGS" CLR_END "\n");
		return;
	}

	printf(CLR_GREEN "ARGS" CLR_END "\n");

	AstCallArg *cur = call->args;
	set_next_sibling(deep(ctx, +1), 1);

	while (cur != NULL) {
		if (cur->next == NULL) {
			set_next_sibling(deep(ctx, +1), 0);
		}
		print_expr(deep(ctx, +2), cur->arg);
		cur = cur->next;
	}
	set_next_sibling(deep(ctx, +1), 0);
}

static void print_type_apply(PrintCtx ctx, const AstTypeAppl *appl) {
	print_tab(ctx);
	printf(CLR_GREEN "TYPE APPLICATION" CLR_END "\n");

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "APPLICANT" CLR_END "\n");

	set_next_sibling(ctx, 1);

	print_expr(deep(ctx, +2), appl->applicant);

	set_next_sibling(ctx, 0);

	print_tab(deep(ctx, +1));

	if (appl->args->arg == NULL) {
		printf(CLR_GREEN "NO ARGS" CLR_END "\n");
		return;
	}

	printf(CLR_GREEN "ARGS" CLR_END "\n");

	AstTypeApplArg *cur = appl->args;
	set_next_sibling(deep(ctx, +1), 1);

	while (cur != NULL) {
		if (cur->next == NULL) {
			set_next_sibling(deep(ctx, +1), 0);
		}
		print_type(deep(ctx, +2), cur->arg);
		cur = cur->next;
	}
	set_next_sibling(deep(ctx, +1), 0);
}

static void print_match_args(PrintCtx ctx, const AstMatchArg *arg) {
	print_tab(deep(ctx, +2));
	printf(CLR_GREEN "ARG ");
	switch (arg->kind) {
	case AST_MATCH_UNKOWN:
		printf(CLR_RED "<UNK>" CLR_END "\n");
		break;
	case AST_MATCH_CAPTURE:
		printf(CLR_YELLOW "CAPTURE" CLR_END "\n");
		print_ident_prefix(deep(ctx, +3), CLR_GREEN "IDENT ", arg->capture);
		break;
	case AST_MATCH_BINDING:
		printf(CLR_YELLOW "BINDING" CLR_END "\n");
		print_ident_prefix(deep(ctx, +3), CLR_GREEN "IDENT ", arg->binding);
		break;
	case AST_MATCH_LITERAL:
		printf(CLR_YELLOW "LITERAL" CLR_END "\n");
		print_tab(deep(ctx, +3));
		printf(CLR_GREEN "VALUE ");
		print_literal(ctx, arg->literal);
		putchar('\n');
		break;
	}
	if (arg->next != NULL) print_match_args(ctx, arg->next);
}

static void print_match(PrintCtx ctx, const AstMatch *match) {
	print_tab(ctx);
	printf(CLR_GREEN "MATCH" CLR_END "\n");
	set_next_sibling(ctx, 1);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "OPERAND" CLR_END "\n");

	print_expr(deep(ctx, +2), match->operand);

	AstMatchArm *arm = match->arms;

	set_next_sibling(ctx, 1);
	while (arm != NULL) {
		if (arm->next == NULL) {
			set_next_sibling(ctx, 0);
		}

		print_ident_prefix(deep(ctx, +1), CLR_GREEN "ARM ", arm->name);

		set_next_sibling(deep(ctx, +1), 1);

		if (arm->args != NULL) {
			print_match_args(ctx, arm->args);
		}

		set_next_sibling(deep(ctx, +1), 0);

		print_tab(deep(ctx, +2));
		printf(CLR_GREEN "EXPR" CLR_END "\n");
		print_expr(deep(ctx, +3), arm->expr);

		arm = arm->next;
	}
	set_next_sibling(ctx, 0);
}

static void print_expr(PrintCtx ctx, const AstExpr *expr) {
	switch (expr->kind) {
	case AST_EXPR_UNKNOWN:
		print_tab(ctx);
		printf(CLR_RED "<UNK>" CLR_END "\n");
		break;

	case AST_EXPR_IDENT:
		print_ident_prefix(ctx, CLR_GREEN "IDENTIFIER ", expr->ident);
		break;

	case AST_EXPR_LITERAL:
		print_literal(ctx, expr->literal);
		break;

	case AST_EXPR_BLOCK:
		print_tab(ctx);
		printf(CLR_GREEN "BLOCK" CLR_END "\n");
		print_expr(deep(ctx, +1), expr->block->body);
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

	case AST_EXPR_BIND:
		print_bind(ctx, expr->bind);
		break;

	case AST_EXPR_WITH:
		print_with(ctx, expr->with);
		break;

	case AST_EXPR_CALL:
		print_call(ctx, expr->call);
		break;

	case AST_EXPR_TYPE_APPLY:
		print_type_apply(ctx, expr->type_appl);
		break;

	case AST_EXPR_INDEX:
		print_tab(ctx);
		printf(CLR_GREEN "INDEX" CLR_END "\n");

		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "BASE" CLR_END "\n");

		set_next_sibling(ctx, 1);

		print_expr(deep(ctx, +2), expr->index->base);

		set_next_sibling(ctx, 0);

		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "VALUE" CLR_END "\n");

		print_expr(deep(ctx, +2), expr->index->index);
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

	case AST_EXPR_MATCH:
		print_match(ctx, expr->match);
		break;
	}
}

static void print_type_list(PrintCtx ctx, const AstTypeList *args) {
	print_tab(ctx);
	printf(CLR_GREEN "ARGS" CLR_END "\n");

	set_next_sibling(ctx, 1);

	while (args != NULL) {
		if (args->next == NULL) {
			set_next_sibling(ctx, 0);
		}
		print_type(deep(ctx, +1), args->arg);
		args = args->next;
	}

	set_next_sibling(ctx, 0);

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

		print_type_list(deep(ctx, +1), type->generic.args);
		break;
	}
}

static void print_struct_field(PrintCtx ctx, const AstStructField *field) {
	print_tab(ctx);
	printf(CLR_GREEN "FIELD" CLR_END "\n");

	print_ident_prefix(deep(ctx, +1), CLR_GREEN "NAME ", field->name);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "TYPE" CLR_END "\n");
	print_type(deep(ctx, +2), field->type);
}

static void print_struct(PrintCtx ctx, const AstStruct *struc) {
	print_ident_prefix(ctx, CLR_GREEN "STRUCT ", struc->name);

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

	print_ident_prefix(deep(ctx, +1), CLR_GREEN "NAME ", param->name);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "TYPE" CLR_END "\n");
	print_type(deep(ctx, +2), param->type);
}

static void print_generic_params(PrintCtx ctx, AstGenericList *generic) {
	print_tab(ctx);
	printf(CLR_GREEN "GENERIC PARAMS" CLR_END "\n");

	while (generic != NULL) {
		print_tab(deep(ctx, +1));
		print_ident(ctx, generic->name);
		putchar('\n');
		generic = generic->next;
	}
}

static void print_func(PrintCtx ctx, const AstFunction *func) {
	print_tab(ctx);
	if (func->decl->is_public) {
		printf(CLR_CYAN "PUB " CLR_END);
	}
	printf(CLR_GREEN "FUNCTION" CLR_END "\n");

	set_next_sibling(ctx, 1);

	print_ident_prefix(deep(ctx, +1), CLR_GREEN "NAME ", func->decl->name);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "RETURNS" CLR_END "\n");
	print_type(deep(ctx, +2), func->decl->return_type);

	if (func->decl->contexts != NULL) {
		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "CONTEXTS" CLR_END "\n");

		AstFunctionContext *context = func->decl->contexts;
		while (context != NULL) {
			print_tab(deep(ctx, +2));
			print_ident(ctx, context->name);
			putchar('\n');
			context = context->next;
		}
	}

	if (func->decl->generics != NULL) {
		print_generic_params(deep(ctx, +1), func->decl->generics);
	}

	AstFunctionParam *param = func->decl->params;
	while (param != NULL) {
		if (param->next == NULL && func->def == NULL) {
			set_next_sibling(ctx, 0);
		}

		print_func_param(deep(ctx, +1), param);
		param = param->next;
	}

	if (func->def == NULL) return;

	set_next_sibling(ctx, 0);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "BLOCK" CLR_END "\n");
	print_expr(deep(ctx, +2), func->def->body);
}

static void print_trait(PrintCtx ctx, const AstTrait *trait) {
	print_tab(ctx);
	printf(CLR_GREEN "TRAIT" CLR_END "\n");

	print_ident_prefix(deep(ctx, +1), CLR_GREEN "NAME ", trait->name);

	if (trait->funcs != NULL) {
		set_next_sibling(ctx, 1);
	}

	if (trait->reqs != NULL) {
		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "REQUIREMENTS" CLR_END "\n");

		set_next_sibling(deep(ctx, +1), 1);
		AstTraitReq *req = trait->reqs;
		while (req != NULL) {
			print_tab(deep(ctx, +2));
			print_ident(ctx, req->name);
			putchar('\n');
			req = req->next;
		}
		set_next_sibling(deep(ctx, +1), 0);
	}

	if (trait->funcs == NULL) return;

	set_next_sibling(ctx, 0);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "MEMBERS" CLR_END "\n");

	set_next_sibling(deep(ctx, +1), 1);

	AstTraitFunction *func = trait->funcs;
	while (func != NULL) {
		if (func->next == NULL) {
			set_next_sibling(deep(ctx, +1), 0);
		}
		print_func(deep(ctx, +2), func->func);
		func = func->next;
	}
	set_next_sibling(deep(ctx, +1), 0);
}

static void print_impl(PrintCtx ctx, const AstImpl *impl) {
	print_tab(ctx);
	printf(CLR_GREEN "IMPL" CLR_END "\n");

	set_next_sibling(ctx, 1);

	if (impl->generics != NULL) {
		print_generic_params(deep(ctx, +1), impl->generics);
	}

	if (impl->funcs == NULL && impl->trait == NULL) {
		set_next_sibling(ctx, 0);
	}

	print_ident_prefix(deep(ctx, +1), CLR_GREEN "SELF ", impl->self);

	if (impl->self_args != NULL) {
		print_type_list(deep(ctx, +1), impl->self_args);
	}

	if (impl->funcs == NULL) {
		set_next_sibling(ctx, 0);
	}

	if (impl->trait != NULL) {
		print_ident_prefix(deep(ctx, +1), CLR_GREEN "TRAIT ", impl->trait);
	}

	if (impl->funcs == NULL) return;

	set_next_sibling(ctx, 0);

	print_tab(deep(ctx, +1));
	printf(CLR_GREEN "MEMBERS" CLR_END "\n");

	set_next_sibling(deep(ctx, +1), 1);

	AstImplFunction *func = impl->funcs;
	while (func != NULL) {
		if (func->next == NULL) {
			set_next_sibling(deep(ctx, +1), 0);
		}
		print_func(deep(ctx, +2), func->func);
		func = func->next;
	}
	set_next_sibling(deep(ctx, +1), 0);
}

static void print_enum_variants(PrintCtx ctx, const AstEnumMember *member) {
	if (member->next == NULL) {
		set_next_sibling(deep(ctx, -1), 0);
	}

	print_ident_prefix(ctx, CLR_GREEN "VARIANT ", member->name);

	set_next_sibling(ctx, 1);

	AstTypeList *cur = member->payload;
	while (cur != NULL) {
		if (cur->next == NULL) {
			set_next_sibling(ctx, 0);
		}
		print_type(deep(ctx, +1), cur->arg);
		cur = cur->next;
	}

	set_next_sibling(ctx, 0);

	if (member->next != NULL) {
		print_enum_variants(ctx, member->next);
	}
}

static void print_enum(PrintCtx ctx, const AstEnum *enumer) {
	print_tab(ctx);
	printf(CLR_GREEN "ENUM" CLR_END "\n");

	print_ident_prefix(deep(ctx, +1), CLR_GREEN "NAME ", enumer->name);

	set_next_sibling(ctx, 1);

	if (enumer->generics != NULL) {
		print_generic_params(deep(ctx, +1), enumer->generics);
	}

	set_next_sibling(ctx, 0);

	if (enumer->members != NULL) {
		print_tab(deep(ctx, +1));
		printf(CLR_GREEN "MEMBERS" CLR_END "\n");

		set_next_sibling(deep(ctx, +1), 1);
		print_enum_variants(deep(ctx, +2), enumer->members);
	}
}

static void print_item(PrintCtx ctx, const AstItem *item) {
	switch (item->kind) {
	case AST_ITEM_UNKNOWN:
		print_tab(ctx);
		printf(CLR_RED "<UNK>" CLR_END "\n");
		break;
	case AST_ITEM_FUNCTION:
		print_func(ctx, item->function);
		break;
	case AST_ITEM_STRUCT:
		print_struct(ctx, item->struc);
		break;
	case AST_ITEM_TRAIT:
		print_trait(ctx, item->trait);
		break;
	case AST_ITEM_IMPL:
		print_impl(ctx, item->impl);
		break;
	case AST_ITEM_ENUM:
		print_enum(ctx, item->enumer);
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
