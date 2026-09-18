#ifndef SOURCE_H
#define SOURCE_H

#include <smew/vec.h>

#include <stdio.h>
#include <stddef.h>

typedef enum {BUF_OK, BUF_ERR} BufResult;

typedef struct {
	const char *filename;
	Vec(char)   buf;
} SourceFile;

BufResult file_read(SourceFile *file, FILE *file_handler);

typedef struct {
	size_t start;
	size_t len;
} Span;

Span span_span(Span left, Span right);
Span zero_span(void);

typedef struct {
	size_t line_offset; // 0-based, in bytes
	size_t line;        // 0-based, in lines
	size_t col;         // 0-based, in bytes
} SourceLocation;

SourceLocation locate_offset(const char *buffer, size_t offset);

#endif
