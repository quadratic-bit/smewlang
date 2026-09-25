#ifndef AST_H
#define AST_H

#include <smew/source.h>
#include <smew/vec.h>

#include <stdint.h>

typedef struct AstExpr  AstExpr;
typedef struct AstType  AstType;
typedef struct AstBlock AstBlock;

typedef struct {
	Span span;
} AstIdent;

typedef enum {
	AST_TYPE_UNKNOWN,

	AST_TYPE_NAME,         // T

	AST_TYPE_POINTER,      // T*
	AST_TYPE_BORROW,       // &T
	AST_TYPE_BORROW_MUT,   // &mut T

	AST_TYPE_ARRAY_FIXED,  // T[N]
	AST_TYPE_ARRAY_DYN,    // T[]

	AST_TYPE_GENERIC       // T(A, B)
} AstTypeKind;

typedef struct AstTypeGeneric AstTypeGeneric;
struct AstTypeGeneric {
	AstType        *arg;
	AstTypeGeneric *next;
};

struct AstType {
	Span span;
	AstTypeKind kind;

	union {
		struct { AstIdent *ident; } name;
		struct { AstType  *inner; } pointer;
		struct { AstType  *inner; } borrow;
		struct { AstType  *inner; } borrow_mut;
		struct { AstType  *inner; } array_dyn;

		struct {
			AstType *inner;
			AstExpr *length;
		} array_fixed;

		struct {
			AstType        *base;
			AstTypeGeneric *args;
		} generic;
	};
};

typedef struct AstCondBlock AstCondBlock;
struct AstCondBlock {
	Span span;
	AstExpr      *cond;
	AstBlock     *block;
	AstCondBlock *next;
};

typedef struct {
	Span span;
	AstCondBlock *conds;
} AstBranch;

typedef struct {
	Span span;
	AstBlock *body;
} AstLoop;

typedef enum {
	AST_OP_BINARY_SEQ,
	AST_OP_BINARY_PLUS,
	AST_OP_BINARY_MINUS,
	AST_OP_BINARY_DIV,
	AST_OP_BINARY_MULT,
	AST_OP_BINARY_ACCESSOR,
	AST_OP_BINARY_ASSIGN,
	AST_OP_BINARY_EQ,
	AST_OP_BINARY_NEQ,
	AST_OP_BINARY_GT,
	AST_OP_BINARY_LT,
	AST_OP_BINARY_GE,
	AST_OP_BINARY_LE,
} AstOpKindBinary;

typedef enum {
	AST_OP_UNARY_NOT,
	AST_OP_UNARY_MINUS,
	AST_OP_UNARY_MOVE,
	AST_OP_UNARY_RETURN,
	AST_OP_UNARY_BORROW,
	AST_OP_UNARY_DEREF,
	AST_OP_UNARY_UNWRAP,
} AstOpKindUnary;

typedef struct {
	Span span;
	AstOpKindBinary op;
	AstExpr *left;
	AstExpr *right;
} AstOpBinary;

typedef struct {
	Span span;
	AstOpKindUnary op;
	AstExpr *operand;
} AstOpUnary;

typedef enum {
	AST_LITERAL_UNIT,
	AST_LITERAL_INT,
	AST_LITERAL_STRUCT,
} AstLiterlKind;

typedef struct AstLiteralStructField AstLiteralStructField;
struct AstLiteralStructField {
	Span      span;
	AstIdent *field;
	AstExpr  *value;
	AstLiteralStructField *next;
};

typedef struct {
	AstType *type;
	AstLiteralStructField *fields;
} AstStructLiteral;

typedef struct {
	Span span;
	AstLiterlKind kind;

	union {
		/* Nothing       Unit; */
		/* Nothing       Int; */
		AstStructLiteral struc;
	};
} AstLiteral;

typedef struct AstCallArg AstCallArg;
struct AstCallArg {
	AstExpr    *arg;
	AstCallArg *next;
};

typedef struct {
	Span span;
	AstExpr    *callee;
	AstCallArg *args;
} AstCall;

typedef struct {
	Span span;
	AstExpr *base;
	AstExpr *index;
} AstIndex;

typedef struct {
	Span span;
	AstType  *type;
	AstIdent *name;
	AstExpr  *value;
} AstBind;

typedef struct {
	Span span;
	AstBind  *bind;
	AstBlock *body;
} AstWith;

typedef struct {
	Span span;
} AstBreak;

typedef enum {
	AST_EXPR_UNKNOWN,
	AST_EXPR_IF,         // done
	AST_EXPR_LOOP,       // done
	AST_EXPR_OP_BINARY,  // done
	AST_EXPR_OP_UNARY,   // done
	AST_EXPR_CALL,       // done
	AST_EXPR_INDEX,      // done
	AST_EXPR_BIND,       // done
	AST_EXPR_WITH,
	AST_EXPR_BREAK,      // done
	AST_EXPR_LITERAL,    // done
	AST_EXPR_IDENT,      // done
} AstExprKind;

struct AstExpr {
	Span span;
	AstExprKind kind;

	union {
		AstBranch   *branch;
		AstLoop     *loop;
		AstOpBinary *op_binary;
		AstOpUnary  *op_unary;
		AstCall     *call;
		AstBind     *bind;
		AstIndex    *index;
		AstWith     *with;
		AstBreak    *brk;
		AstLiteral  *literal;
		AstIdent    *ident;
	};
};

struct AstBlock {
	Span     span;
	AstExpr *body;
};

typedef struct AstFunctionParam AstFunctionParam;

struct AstFunctionParam {
	Span      span;
	AstType  *type;
	AstIdent *name;
	AstFunctionParam *next;
};

typedef struct {
	Span span;
	int  is_public;

	AstIdent *name;
	AstFunctionParam *params;
	AstType  *return_type;
	AstBlock *block;
} AstFunction;

typedef struct AstStructField AstStructField;
struct AstStructField {
	Span            span;
	AstIdent       *name;
	AstType        *type;
	AstStructField *next;
};

typedef struct {
	Span      span;
	AstIdent *name;
	AstStructField *fields;
} AstStruct;

typedef enum {
	AST_ITEM_FUNCTION,
	AST_ITEM_STRUCT,
} AstItemKind;

typedef struct {
	Span span;
	AstItemKind kind;

	union {
		AstFunction *function;
		AstStruct   *struc;
	};
} AstItem;

typedef struct {
	Vec(AstItem *) items;
} Ast;

void print_ast(const char *src, const Ast *ast);

#endif
