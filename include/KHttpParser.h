#ifndef KHTTPPARSER_H
#define KHTTPPARSER_H
#include "global.h"

typedef enum {
	kgl_parse_error,
	kgl_parse_success,
	kgl_parse_finished,
	kgl_parse_want_read,
	kgl_parse_continue
} kgl_parse_result;

typedef enum {
	kgl_header_failed,
	kgl_header_success,
	kgl_header_no_insert,
	kgl_header_insert_begin
} kgl_header_result;

typedef struct {
	char *val;
	int val_len;
} khttp_field;

typedef struct {
	char *attr;
	char *val;
	int attr_len;
	int val_len;
	uint8_t is_first : 1;
	uint8_t request_line : 1;
} khttp_parse_result;

typedef struct {
	char *data;
	int len;
	int checked;
} khttp_parse_body_result;

typedef struct {
	int header_len;
	uint8_t started : 1;
	uint8_t finished : 1;
	uint8_t first_same : 1;
} khttp_parser;

kgl_parse_result khttp_parse(khttp_parser *parser, char **buf, int *len, khttp_parse_result *rs);

#endif

