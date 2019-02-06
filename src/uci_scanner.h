#ifndef UCI_SCANNER_H_
#define UCI_SCANNER_H_

#define YY_NO_INPUT

int uci_scanner_lex(void);
void uci_scanner_restart (FILE *input_file);

#ifndef YY_TYPEDEF_YY_SIZE_T
#define YY_TYPEDEF_YY_SIZE_T
typedef unsigned int yy_size_t;
#endif

extern yy_size_t uci_scanner_leng;
extern char *uci_scanner_text;

enum _uci_match_type {
	EOF_TYPE = -1,
	UNMATCHED = 0,
	EMPTY_LINE,
	LINE_FEED,
	UCI_OK,
	UCI_READY,
	UCI_ID_NAME,
    UCI_ID_AUTHOR,
	UCI_OPTION,
	UCI_INFO,
	UCI_BEST_MOVE_NONE,
	UCI_BEST_MOVE_WITH_PONDER,
	UCI_BEST_MOVE
};

#endif /* UCI_SCANNER_H_ */
