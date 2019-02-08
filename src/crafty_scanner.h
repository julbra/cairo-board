/*
 * crafty_scanner.h
 *
 *  Created on: 7 Dec 2009
 *      Author: hts
 */

#ifndef CRAFTY_SCANNER_H_
#define CRAFTY_SCANNER_H_

#define YY_NO_INPUT

int crafty_scanner_lex(void);
void crafty_scanner_restart (FILE *input_file);

#ifndef YY_TYPEDEF_YY_SIZE_T
#define YY_TYPEDEF_YY_SIZE_T
typedef unsigned int yy_size_t;
#endif

#ifndef __APPLE_CC__
extern int crafty_scanner_leng;
#else
extern yy_size_t crafty_scanner_leng;
#endif

extern char *crafty_scanner_text;

enum _crafty_match_type {
	EOF_TYPE = -1,
	UNMATCHED = 0,
	EMPTY_LINE,
	LINE_FEED,
	CRAFTY_MOVED,
	CRAFTY_HINT,
	CRAFTY_PONDERED,
};

#endif /* CRAFTY_SCANNER_H_ */
