
#ifndef __SAN_PARSER_H__
#define __SAN_PARSER_H__

#define YY_NO_INPUT
#define YY_NO_UNPUT

int san_scanner_lex(void);
void san_scanner_restart (FILE *input_file);

int char_to_type(int whose_turn, char);
char type_to_char(int);

extern int promo_type;
extern int type;
extern char currentMoveString[];

enum _san_match_type {
	SAN_EOF_TYPE = -1,
	SAN_UNMATCHED = 0,
	MATCHED_MOVE = 1,
	MATCHED_TAG = 2,
	MATCHED_END_TOKEN,
};

#endif
