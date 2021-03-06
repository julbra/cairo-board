
#ifndef __ICS_PARSER_H__
#define __ICS_PARSER_H__

#define YY_NO_INPUT

int ics_scanner_lex(void);
void ics_scanner_restart (FILE *input_file);

extern int ics_scanner_leng;
YY_BUFFER_STATE ics_scanner__scan_bytes(const char *bytes, int len);

enum _ics_match_type {
	EOF_TYPE = -1,
	UNMATCHED = 0,
	EMPTY_LINES,
	LINE_CONTINUED,
	LOGIN_PROMPT,
	PASSWORD_PROMPT,
	INVALID_PASSWORD,
	CONFIRM_GUEST_LOGIN_PROMPT,
	GOT_LOGIN,
	FICS_PROMPT,
	BOARD_12,
	CREATE_MESSAGE,
	GAME_START,
	GAME_RESUME,
	GAME_END,
	WILL_ECHO,
	WONT_ECHO,
	MOVE_LIST_START,
	MOVE_LIST_UNDERLINE,
	MOVE_LIST_END,
	MOVE_LIST_FULL_MOVE,
	MOVE_LIST_WHITE_PLY,
	OBSERVE_START,
	OBSERVE_HEADER,
	CHALLENGE,
	CHANNEL_CHAT,
	PRIVATE_TELL,
	OFFER_FROM,
	OFFER_REMOVED,
	OFFER_TO,
	MY_CHANNELS_HEADER,
	MY_CHANNELS_LINE,
	CHANNEL_REMOVED,
	CHANNEL_ADDED,
	FOLLOWING
};

#endif

