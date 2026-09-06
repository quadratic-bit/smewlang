#ifndef LINE_H
#define LINE_H

#include <stddef.h>

typedef struct {
	size_t line_offset; // 0-based, in bytes
	size_t line;        // 0-based, in lines
	size_t col;         // 0-based, in bytes
} SourceLocation;

SourceLocation locate_offset(char *buffer, size_t offset);

#endif
