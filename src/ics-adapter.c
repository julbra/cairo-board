#include "ics-adapter.h"
#include "cairo-board.h"
#include "crafty-adapter.h"
#include "ics_scanner.h"
#include "channels.h"
#include "configuration.h"
#include "uci-adapter.h"
#include "san_scanner.h"
#include "chess-backend.h"
#include "drawing-backend.h"
#include "netstuff.h"

/* How much data we read from ICS at once
 * Try smaller values to test the stitching mechanism */
// #define ICS_BUFF_SIZE 32
#define ICS_BUFF_SIZE 1024

static int finished_parsing_moves = 0;
static int requested_times = 0;
static int init_time;
static int increment;
static char current_players[2][128];
static char current_ratings[2][128];
static int my_channels_number;
static int parser_state = FREE_PARSER;
static char last_user[32] = {[0 ... 31] = 0};
static int last_channel_number;
static char *last_message;
static char my_handle[128];
static int requested_moves = 0;
static int requested_start = 0;
static int got_header = 0;
static int parsed_plys = 0;

static pthread_t ics_reader_thread;
static pthread_t ics_buff_parser_thread;

char my_login[128];
char my_password[128];
char following_player[32];

bool ics_logged_in = false;
void * icsPr;
int ics_socket;
int ics_fd;
int ics_data_pipe[2];

bool my_channels_requested = false;
bool got_my_channels_header = false;

bool invalid_password = false;

/* From ICC specification
"<12> rnbqkb-r pppppppp -----n-- -------- ----P--- -------- PPPPKPPP RNBQ-BNR B -1 0 0 1 1 0 7 Newton Einstein 1 2 12 39 39 119 122 2 K/e1-e2 (0:06) Ke2 0"

This string always begins on a new line, and there are always exactly 31 non-
empty fields separated by blanks. The fields are:

* the string "<12>" to identify this line.
* eight fields representing the board position.  The first one is White's
  8th rank (also Black's 1st rank), then White's 7th rank (also Black's 2nd),
  etc, regardless of who's move it is.
* colour whose turn it is to move ("B" or "W")
* -1 if the previous move was NOT a double pawn push, otherwise the chess
  board file  (numbered 0--7 for a--h) in which the double push was made
* can White still castle short? (0=no, 1=yes)
* can White still castle long?
* can Black still castle short?
* can Black still castle long?
* the number of moves made since the last irreversible move.  (0 if last move
  was irreversible.  If the value is >= 100, the game can be declared a draw
  due to the 50 move rule.)
* The game number
* White's name
* Black's name
* my relation to this game:
	-3 isolated position, such as for "ref 3" or the "sposition" command
	-2 I am observing game being examined
	 2 I am the examiner of this game
	-1 I am playing, it is my opponent's move
	 1 I am playing and it is my move
	 0 I am observing a game being played
* initial time (in seconds) of the match
* increment In seconds) of the match
* White material strength
* Black material strength
* White's remaining time
* Black's remaining time
* the number of the move about to be made (standard chess numbering -- White's
  and Black's first moves are both 1, etc.)
* verbose coordinate notation for the previous move ("none" if there were
  none) [note this used to be broken for examined games]
* time taken to make previous move "(min:sec)".
* pretty notation for the previous move ("none" if there is none)
* flip field for board orientation: 1 = Black at bottom, 0 = White at bottom.

In the future, new fields may be added to the end of the data string, so
programs should parse from left to right.


*/

//<12> rnbqkb-r pppppppp -----n-- -------- ----P--- -------- PPPPKPPP RNBQ-BNR B -1 0 0 1 1 0 7 Newton Einstein 1 2 12 39 39 119 122 2 K/e1-e2 (0:06) Ke2 0

#define STYLE_12_PATTERN "%71c %c %d %d %d %d %d %d %d %s %s %d %d %d %d %d %d %d %d %s %s %s %d %d"
// NB: 71 comes from 64 characters + 7 spaces

/* Note: we have removed the '<12> ' when we reach this function so we get sth like
-nr---k- r---qpp- --p----p p--pN--- -p-PN--- ----P--- PPQ--PPP R-R---K- B -1 0 0 0 0 0 307 pgayet radmanilko 0 3 0 32 29 140 128 20 N/c5-e4 (0:03) Nxe4 0 1 477
*/

int parse_board_tries = 0;
char first_board_chunk[256]; // board is about 150 chars on average so 256 is safe

int parse_board12(char *string_chunk) {
	int gamenum, relation, basetime, increment, ics_flip = 0;
	int n, moveNum, white_stren, black_stren, white_time, black_time;
	int double_push, castle_ws, castle_wl, castle_bs, castle_bl, fifty_move_count;
	char to_play, board_chars[72];
	char str[MOVE_BUFF_SIZE], san_move[MOVE_BUFF_SIZE], elapsed_time[MOVE_BUFF_SIZE];
	char b_name[121], w_name[121];
	int ticking = 2;

	char *board12_string;

	memset(board_chars, 0, sizeof(board_chars));
	if (parse_board_tries) { // second chunk came in
		board12_string = first_board_chunk;
		strcat(board12_string, string_chunk);
	}
	else {
		board12_string = string_chunk; // null terminated
	}
	debug("Board 12 string: '%s'\n", board12_string);

	n = sscanf(board12_string, STYLE_12_PATTERN,
	           board_chars, &to_play, &double_push,
	           &castle_ws, &castle_wl, &castle_bs,
	           &castle_bl, &fifty_move_count, &gamenum,
	           w_name, b_name, &relation,
	           &basetime, &increment, &white_stren,
	           &black_stren, &white_time, &black_time,
	           &moveNum, str, elapsed_time,
	           san_move, &ics_flip, &ticking);

	// We need fields up until the Pretty Move (if the flip flop status bit and the trailing characters were chopped, tough)
	if (n < 23) {
		if (!parse_board_tries) {
			parse_board_tries++;
			memcpy(first_board_chunk, board12_string, strlen(board12_string));
			first_board_chunk[strlen(board12_string)] = 0; // NULL terminate
			return 1;
		} else {
			fprintf(stderr, "FAILED to parse Board 12 String:\n\"%s\"\n", board12_string);
			parse_board_tries = 0;
			return -1;
		}
	} else {
		debug("scanned %d fields\n", n);
		parse_board_tries = 0;
		debug("Successfully parsed Board 12:\n");
		debug("\tBlack's name: %s\n", b_name);
		debug("\tWhite's name: %s\n", w_name);
		debug("\tIt is %s's move\n", (to_play == 'B' ? "Black" : "White"));
		debug("\tWhite's time: %ds\n", white_time);
		debug("\tBlack's time: %ds\n", black_time);
		debug("\tMy relation: %d\n", relation);
		debug("\tBoard chars: '%s'\n", board_chars);
		debug("\tSAN move: '%s'\n\n", san_move);

		// Did we ask for times?
		if (requested_times) {
			requested_times = 0;
			update_clocks(main_clock, white_time, black_time, true);
			if (!clock_started && moveNum >= 2) {
				clock_started = 1;
				start_one_clock(main_clock, (to_play == 'W')?0:1);
				debug("start clock\n");
			}
			return 0;
		}

		// game and clocks started: update the clocks
		if (game_started && clock_started) {
			update_clocks(main_clock, white_time, black_time, true);
		}

		// game started and not observing: set last move and swap clocks if needed
		if (game_started && relation) {
			set_last_move(san_move);
			if (clock_started) {
				start_one_stop_other_clock(main_clock, (to_play == 'W') ? 0 : 1, true);
			}
		}


		/* NOTE: on ICS the clock starts after black's first move.
		 * This is for obvious reasons due to the nature of online games
		 * and the challenging/seek system.
		 * This is different from the official FIDE chess rules which state that:
		 * "At the time determined for the start of the game, the clock of the
		 * player who has the white pieces is started.*/
		if ( !clock_started && moveNum == 2 && relation && game_started && to_play == 'W' ) {
			start_one_clock(main_clock, 0);
			clock_started = 1;
		}


		/*
		 * my relation to this game:
		 *     -3 isolated position, such as for "ref 3" or the "sposition" command
		 *     -2 I am observing game being examined
		 *      2 I am the examiner of this game
		 *     -1 I am playing, it is my opponent's move
		 *      1 I am playing and it is my move
		 *      0 I am observing a game being played
		 * */
		switch (relation) {
			case 1:
				if (!strncmp("none", san_move, 4)) {
					break;
				}
				debug("Emitting got-move signal while playing\n");
				g_signal_emit_by_name(board, "got-move");
				break;
			case 0:
			case -2:
			case 2:
				if (clock_started) {
					debug("Swapping clocks here\n");
					start_one_stop_other_clock(main_clock, (to_play == 'W') ? 0 : 1, true);
				}
				if (gamenum == my_game && finished_parsing_moves) {
					if (!game_started) {
						game_started = true;
					}
					if (!clock_started && moveNum >= 2) {
						clock_started = 1;
						update_clocks(main_clock, white_time, black_time, true);
						debug("Starting a clock here\n");
						start_one_clock(main_clock, (to_play == 'W') ? 0 : 1);
					}
					set_last_move(san_move);
					debug("Emitting got-move signal while observing\n");
					g_signal_emit_by_name(board, "got-move");
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

/* "Creating: julbra (1911) lesio ( 866) rated blitz 3 0" */
int parse_create_message(char *message, gboolean *rated, int *init_time, int *increment) {

	if (ics_scanner_leng < 11) {
		fprintf(stderr, "Bug in ICS parser, token length for Create message should be more than 11\n");
		exit(1);
	}
	message += 10;
	int ret = 0;

	memset(current_players[0], 0, 128);
	memset(current_players[1], 0, 128);
	memset(current_ratings[0], 0, 128);
	memset(current_ratings[1], 0, 128);

	char *first_space = strchr(message, ' ');
	if (first_space) {
		memcpy(current_players[0], message, first_space-message);
		ret++;
	}
	char *first_left_brace = strchr(message, '(');
	char *first_right_brace = strchr(message, ')');
	if (first_left_brace && first_right_brace) {
		while (first_left_brace[1] == ' ' && first_left_brace < first_right_brace) {
			debug("Rating starting with space?\n");
			first_left_brace++;
		}
		memcpy(current_ratings[0], first_left_brace+1, first_right_brace-first_left_brace-1);
		ret++;
	}

	char *third_space = strchr(first_right_brace+2, ' ');
	if (third_space) {
		memcpy(current_players[1], first_right_brace+2, third_space-first_right_brace-2);
		ret++;
	}

	char *second_left_brace = strchr(first_right_brace+1, '(');
	char *second_right_brace = strchr(first_right_brace+1, ')');
	if (second_left_brace && second_right_brace) {
		while (second_left_brace[1] == ' ' && second_left_brace < second_right_brace) {
			debug("Rating starting with space?\n");
			second_left_brace++;
		}
		memcpy(current_ratings[1], second_left_brace+1, second_right_brace-second_left_brace-1);
		ret++;
	}

	*rated = memcmp("rated", second_right_brace+2, 5)?FALSE:TRUE;
	debug("rated: '%d'\n", *rated);

	char *fourth_space = strchr(second_right_brace+2, ' ');
	char *fifth_space = strchr(fourth_space+1, ' ');
	char game_mode[128];
	memset(game_mode, 0, 128);
	if (fifth_space) {
		memcpy(game_mode, fourth_space+1, fifth_space-fourth_space-1);
		debug("game_mode: '%s'\n", game_mode);
		ret++;
	}

	char *inc;
	*init_time = strtol(fifth_space, &inc, 10);
	debug("init_time: '%d'\n", *init_time);

	*increment = strtol(inc, NULL, 10);
	debug("increment: '%d'\n", *increment);

	return ret;
}

/*
Challenge: GuestMZPD (----) GuestKYYK (----) unrated standard 15 2.
Challenge: GuestJRBS (----) [black] GuestSBNL (----) unrated blitz 5 0.
*/
int parse_challenge_message(char *message, char w_name[128], char b_name[128], char w_rating[5], char b_rating[5], gboolean *rated, int *init_time, int *increment) {

	if (ics_scanner_leng < 12) {
		fprintf(stderr, "Bug in ICS parser, token length for Challenge message should be more than 12\n");
		exit(1);
	}
	message += 11;
	int ret = 0;

	memset(w_name, 0, 128);
	memset(b_name, 0, 128);
	memset(w_rating, 0, 5);
	memset(b_rating, 0, 5);

	char *first_space = strchr(message, ' ');
	if (first_space) {
		memcpy(w_name, message, first_space-message);
	}
	char *first_left_brace = strchr(message, '(');
	char *first_right_brace = strchr(message, ')');
	if (first_left_brace && first_right_brace) {
		while (first_left_brace[1] == ' ' && first_left_brace < first_right_brace) {
			debug("Rating starting with space?\n");
			first_left_brace++;
		}
		memcpy(w_rating, first_left_brace+1, first_right_brace-first_left_brace-1);
	}

	if (*(first_right_brace+2) == '[') {
		debug("*(first_right_brace+2) == '['\n");
		if (!memcmp(first_right_brace+2, "[black]", 7)) {
			debug("black\n");
			ret = 1;
		}
		else if (!memcmp(first_right_brace+2, "[white]", 7)) {
			debug("white\n");
			ret = 2;
		}
		first_right_brace += 8;
	}
	char *third_space = strchr(first_right_brace+2, ' ');
	if (third_space) {
		memcpy(b_name, first_right_brace+2, third_space-first_right_brace-2);
	}

	char *second_left_brace = strchr(first_right_brace+1, '(');
	char *second_right_brace = strchr(first_right_brace+1, ')');
	if (second_left_brace && second_right_brace) {
		while (second_left_brace[1] == ' ' && second_left_brace < second_right_brace) {
			debug("Rating starting with space?\n");
			second_left_brace++;
		}
		memcpy(b_rating, second_left_brace+1, second_right_brace-second_left_brace-1);
	}

	*rated = memcmp("rated", second_right_brace+2, 5)?FALSE:TRUE;
	debug("rated: '%d'\n", *rated);

	char *fourth_space = strchr(second_right_brace+2, ' ');
	char *fifth_space = strchr(fourth_space+1, ' ');
	char game_mode[128];
	memset(game_mode, 0, 128);
	if (fifth_space) {
		memcpy(game_mode, fourth_space+1, fifth_space-fourth_space-1);
		debug("game_mode: '%s'\n", game_mode);
	}

	char *inc;
	*init_time = strtol(fifth_space, &inc, 10);
	debug("init_time: '%d'\n", *init_time);

	*increment = strtol(inc, NULL, 10);
	debug("increment: '%d'\n", *increment);

	return ret;
}

/*You are now observing game 233.*/
long parse_observe_start_message(char *message) {
	if (ics_scanner_leng < 27) {
		fprintf(stderr, "Bug in ICS parser, token length for Observe Start message should be more than 26\n");
		exit(1);
	}
	return strtol(message + 27, NULL, 10);
}

/*
Game 127: ImAGoon (1910) FDog (2025) rated standard 15 0
*/
int parse_observe_header(char* message, long *game_num, char w_name[128], char b_name[128], char w_rating[5], char b_rating[5], gboolean *rated, int *init_time, int *increment) {
	if (ics_scanner_leng < 6) {
		fprintf(stderr, "Bug in ICS parser, token length for Observe Header message should be more than 5\n");
		exit(1);
	}

	int ret = 0;

	*game_num = 0;
	char *colon;
	*game_num = strtol(message+5, &colon, 10);
	if (*game_num) ret++;

	colon += 2;

	char *first_space = strchr(colon, ' ');
	if (first_space) {
		memcpy(w_name, colon, first_space-colon);
		debug("w_name: '%s'\n", w_name);
		ret++;
	}
	char *first_left_brace = strchr(first_space, '(');
	char *first_right_brace = strchr(first_space, ')');
	if (first_left_brace && first_right_brace) {
		while (first_left_brace[1] == ' ' && first_left_brace < first_right_brace) {
			debug("Rating starting with space?\n");
			first_left_brace++;
		}
		memcpy(w_rating, first_left_brace+1, first_right_brace-first_left_brace-1);
		debug("w_rating: '%s'\n", w_rating);
		ret++;
	}

	char *third_space = strchr(first_right_brace+2, ' ');
	if (third_space) {
		memcpy(b_name, first_right_brace+2, third_space-first_right_brace-2);
		debug("b_name: '%s'\n", b_name);
		ret++;
	}

	char *second_left_brace = strchr(first_right_brace+2, '(');
	char *second_right_brace = strchr(first_right_brace+2, ')');
	if (second_left_brace && second_right_brace) {
		while (second_left_brace[1] == ' ' && second_left_brace < second_right_brace) {
			debug("Rating starting with space?\n");
			second_left_brace++;
		}
		memcpy(b_rating, second_left_brace+1, second_right_brace-second_left_brace-1);
		debug("b_rating: '%s'\n", b_rating);
		ret++;
	}

	*rated = memcmp("rated", second_right_brace+2, 5)?FALSE:TRUE;
	debug("rated: '%d'\n", *rated);
	ret++;

	char *fourth_space = strchr(second_right_brace+2, ' ');
	char *fifth_space = strchr(fourth_space+1, ' ');
	char game_mode[128];
	memset(game_mode, 0, 128);
	if (fifth_space) {
		memcpy(game_mode, fourth_space+1, fifth_space-fourth_space-1);
		debug("game_mode: '%s'\n", game_mode);
		ret++;
	}

	char *inc;
	*init_time = strtol(fifth_space, &inc, 10);
	debug("init_time: '%d'\n", *init_time);

	*increment = strtol(inc, NULL, 10);
	debug("increment: '%d'\n", *increment);
	return ret;
}

/*
{Game 221 (julbra vs. Przemekchess) Creating rated blitz match.}
*/
int parse_start_message(char *message, long *game_num, char w_name[128], char b_name[128]) {

	if (ics_scanner_leng < 5) {
		fprintf(stderr, "Bug in ICS parser, token length for Start message should be more than 5\n");
		exit(1);
	}

	memset(w_name, 0, 128);
	memset(b_name, 0, 128);

	int ret = 0;

	char *first_space;
	*game_num = strtol(message+5, &first_space, 10);
	if (*game_num > 0) {
		debug("Got game number %ld\n", *game_num);
		ret++;
	}

	char *second_space = strchr(first_space+2, ' ');
	if (second_space) {
		memcpy(w_name, first_space+2, second_space-first_space-2);
		debug("Got white name %s\n", w_name);
		ret++;
	}

	char *third_space = strchr(second_space+1, ' ');
	char *first_left_brace = strchr(second_space+1, ')');
	if (third_space && first_left_brace) {
		memcpy(b_name, third_space+1, first_left_brace-third_space-1);
		debug("Got black name %s\n", b_name);
		ret++;
	}

	return ret;
}

/*
{Game 43 (jennyh vs. julbra) Continuing rated standard match.}
*/
int parse_resume_message(char *message, long *game_num, char w_name[128], char b_name[128]) {

	if (ics_scanner_leng < 6) {
		fprintf(stderr, "Bug in ICS parser, token length for Resume message should be more than 6\n");
		exit(1);
	}

	memset(w_name, 0, 128);
	memset(b_name, 0, 128);

	int ret = 0;
	message += 6;
	char *first_space;
	*game_num = strtol(message, &first_space, 10);
	if (*game_num > 0) {
		debug("Got game number %ld\n", *game_num);
		ret++;
	}

	char *second_space = strchr(first_space+2, ' ');
	if (second_space) {
		memcpy(w_name, first_space+2, second_space-first_space-2);
		debug("Got white name %s\n", w_name);
		ret++;
	}

	char *third_space = strchr(second_space+1, ' ');
	char *first_right_brace = strchr(second_space+1, ')');
	if (third_space && first_right_brace) {
		memcpy(b_name, third_space+1, first_right_brace-third_space-1);
		debug("Got black name %s\n", b_name);
		ret++;
	}
	return ret;
}

int am_interested_in_game(long game_num) {
	return game_num == my_game;
}

int parse_end_message(char *message, char end_token[32]) {

	if (ics_scanner_leng < 5) {
		fprintf(stderr, "Bug in ICS parser, token length for Start message should be more than 5\n");
		exit(1);
	}

	char w_name[128];
	char b_name[128];
	long game_num;
	memset(w_name, 0, 128);
	memset(b_name, 0, 128);

	int ret = 0;

	char *first_space;
	game_num = strtol(message+5, &first_space, 10);
	if (!am_interested_in_game(game_num)) {
		// ignore this message
		debug("Ignoring endmessage for game %ld\n", game_num);
		return 0;
	}
	if (game_num > 0) {
		debug("Got game number %ld\n", game_num);
		ret++;
	}

	char *second_space = strchr(first_space+2, ' ');
	if (second_space) {
		memcpy(w_name, first_space+2, second_space-first_space-2);
		debug("Got white name %s\n", w_name);
		ret++;
	}

	char *third_space = strchr(second_space+1, ' ');
	char *first_left_brace = strchr(second_space+1, ')');
	if (third_space && first_left_brace) {
		memcpy(b_name, third_space+1, first_left_brace-third_space-1);
		debug("Got black name %s\n", b_name);
		ret++;
	}
	char *last_space = strrchr(message, ' ');
	if (last_space) {
		memset(end_token, 0, 32);
		strncpy(end_token, last_space+1, 32);
		debug("end token %s\n", end_token);
		ret++;
	}

	return ret;
}

/*
Frubes(50): nice, just gobbledy gook to me though
GuestRRHQ(U)(53): thanks I'll avoid you
SomeAdmin(*)(TM)(SR)(50): thanks I'll avoid you
*/
int parse_channel_chat(char *ics_scanner_text, char *user, char *message) {
	char *first_left_brace = strchr(ics_scanner_text, ':');
	first_left_brace--;
	while(*first_left_brace != '(') {
		first_left_brace--;
	}
	char *first_right_brace;
	long chan_num = strtol(first_left_brace+1, &first_right_brace, 10);
	memcpy(user, ics_scanner_text, first_left_brace-ics_scanner_text);
	/* N.B. we know that our scanner returns null terminated Strings */
	strcpy(message, first_right_brace+3);
	return (int) chan_num;
}

/*
jennyh tells you: hello
*/
void parse_private_tell(char *ics_scanner_text, char *user, char *message) {
	char *first_character = strchr(ics_scanner_text, ':');
	/* N.B. we know that our scanner returns null terminated Strings */
	memcpy(user, ics_scanner_text, first_character-ics_scanner_text-10);
	strcpy(message, first_character+1);
}

void set_fics_variables(void) {
	/* set ivariables */
	send_to_ics("iset defprompt\n");
	send_to_ics("iset pendinfo\n");
	send_to_ics("iset nowrap\n");
	send_to_ics("iset lock\n");

	/* set style 12 now */
	send_to_ics("set style 12\n");
	/* set autoflag now */
	send_to_ics("set autoflag 1\n");

}

void request_my_channels(void) {
	my_channels_requested = TRUE;
	send_to_ics("=chan\n");
}

/*
-- channel list: 29 channels --
*/
int parse_my_channels_header(char *message) {
	char *first_digit = strchr(message, ':');
	long chan_num = strtol(first_digit+1, NULL, 10);
	return (int)chan_num;
}

// gtk login widgetry
GtkWidget *login_dialog;
GtkWidget *login;
GtkWidget *password;
GtkWidget *save_login;
GtkWidget *auto_login;
GtkWidget *info_label;

void close_login_dialog(bool lock_threads) {
	if (lock_threads) {
		gdk_threads_enter();
	}
	gtk_widget_destroy(login_dialog);
	if (lock_threads) {
		gdk_threads_leave();
	}
}

void save_login_toggle_button_callback(GtkWidget *widget, gpointer data) {
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (widget))) {
		gtk_widget_set_sensitive(auto_login, TRUE);
	} else {
		gtk_widget_set_sensitive(auto_login, FALSE);
	}
}

void toggle_login_box(bool enable) {
	gtk_widget_set_sensitive(login, enable);
	gtk_widget_set_sensitive(password, enable);
	gtk_widget_set_sensitive(save_login, enable);
	gtk_widget_set_sensitive(auto_login, enable);
	if (info_label) {
		gtk_widget_set_sensitive(info_label, enable);
	}
	gtk_widget_set_sensitive(gtk_dialog_get_action_area(GTK_DIALOG(login_dialog)), enable);
}

void try_login(void) {
	memset(my_login, 0, 128);
	strncpy(my_login, gtk_entry_get_text(GTK_ENTRY(login)), 128);
	my_login[strlen(my_login)] = '\n';
	memset(my_password, 0, 128);
	strncpy(my_password, gtk_entry_get_text(GTK_ENTRY(password)), 128);
	my_password[strlen(my_password)] = '\n';
	if (!info_label) {
		info_label = gtk_label_new("");
		GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG (login_dialog));
		gtk_box_pack_start(GTK_BOX(content_area), info_label, FALSE, FALSE, 5);
		gtk_widget_show(info_label);
	}
	gtk_label_set_text(GTK_LABEL(info_label), "Logging in...");

	send_to_ics(my_login);
	toggle_login_box(false);
}

void handle_login_box_response(GtkDialog *dialog, gint response) {
	switch (response) {
		case GTK_RESPONSE_ACCEPT:
			try_login();
			break;
		default:
			gtk_widget_destroy(login_dialog);
			break;
	}
}

GtkWidget *create_login_box(void) {
	GtkWidget *login_dialog = gtk_dialog_new();
	gtk_window_set_modal(GTK_WINDOW(login_dialog), FALSE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW (login_dialog), TRUE);
	gtk_window_set_title(GTK_WINDOW(login_dialog), "Login to FICS");
	gtk_window_set_transient_for(GTK_WINDOW(login_dialog), GTK_WINDOW(main_window));


	GtkWidget *login_button = gtk_dialog_add_button(GTK_DIALOG(login_dialog), "Login", GTK_RESPONSE_ACCEPT);
	gtk_button_set_image(GTK_BUTTON(login_button), gtk_image_new_from_icon_name("gtk-apply", GTK_ICON_SIZE_MENU));

	GtkWidget *decline_button = gtk_dialog_add_button(GTK_DIALOG(login_dialog), "Cancel", GTK_RESPONSE_REJECT);
	gtk_button_set_image(GTK_BUTTON(decline_button), gtk_image_new_from_icon_name("gtk-cancel", GTK_ICON_SIZE_MENU));

	GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG (login_dialog));
	GtkWidget *login_label = gtk_label_new("login:");
	gtk_misc_set_alignment(GTK_MISC(login_label), 0, .5);
	login = gtk_entry_new();
	GtkWidget *password_label = gtk_label_new("password:");
	gtk_misc_set_alignment(GTK_MISC(password_label), 0, .5);
	password = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(password), FALSE);

	GtkWidget *table = gtk_table_new(3, 2, FALSE);
	int xattach_flags = GTK_FILL;
	gtk_table_attach (GTK_TABLE(table), login_label,
	                  0, 1, 0, 1, xattach_flags, GTK_FILL | GTK_EXPAND, 10, 2);
	gtk_table_attach (GTK_TABLE(table), login,
	                  1, 2, 0, 1, xattach_flags, GTK_FILL | GTK_EXPAND, 10, 2);
	gtk_table_attach (GTK_TABLE(table), password_label,
	                  0, 1, 1, 2, xattach_flags, GTK_FILL | GTK_EXPAND, 10, 2);
	gtk_table_attach (GTK_TABLE(table), password,
	                  1, 2, 1, 2, xattach_flags, GTK_FILL | GTK_EXPAND, 10, 2);

	save_login = gtk_check_button_new_with_label("Remember me");
	g_signal_connect (G_OBJECT(save_login), "toggled", G_CALLBACK(save_login_toggle_button_callback), NULL);
	auto_login = gtk_check_button_new_with_label("Automatically login");

	GtkWidget *ticks_hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(ticks_hbox), save_login, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ticks_hbox), auto_login, FALSE, FALSE, 0);
	gtk_table_attach (GTK_TABLE(table), ticks_hbox,
	                  0, 2, 2, 3, xattach_flags, GTK_FILL | GTK_EXPAND, 10, 2);

	/* parse configuration and set values */
	if (get_save_login()) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_login), TRUE);
		gtk_entry_set_text(GTK_ENTRY(login), get_login());
		gtk_entry_set_text(GTK_ENTRY(password), get_password());
		if (get_auto_login()) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_login), TRUE);
		}
	}
	else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_login), FALSE);
		gtk_widget_set_sensitive(auto_login, FALSE);
	}

	g_signal_connect_swapped(login_dialog, "response", G_CALLBACK(handle_login_box_response), NULL);

	/* Add the table and check boxes, and show. */
	gtk_box_pack_end(GTK_BOX(content_area), table, FALSE, FALSE, 0);
	gtk_widget_show_all(login_dialog);

	return login_dialog;
}

void show_login_dialog(bool lock_threads) {
	if (lock_threads) {
		gdk_threads_enter();
	}

	if (!login_dialog) {
		login_dialog = create_login_box();
	}

	toggle_login_box(false);

	if (invalid_password) {
		debug("Check Username and Password\n");
		char *markup = g_markup_printf_escaped("<span weight=\"bold\" foreground=\"red\">%s</span>", "Invalid password!");
		gtk_label_set_markup(GTK_LABEL(info_label), markup);
		g_free(markup);
	}

	/* do auto-login now */
	if (get_auto_login() && !invalid_password) {
		debug("Auto-Login...\n");
		if (!get_save_login()) {
			fprintf(stderr, "bug in configuration, get_auto_login was set but get_save_login wasn't!\n");
			exit(1);
		}
		try_login();
	} else {
		invalid_password = FALSE;
		toggle_login_box(true);
	}
	if (lock_threads) {
		gdk_threads_leave();
	}
}

static gboolean handle_accept_decline_response(GtkWidget *pWidget,  gint response_id) {
	switch(response_id) {
		case GTK_RESPONSE_ACCEPT:
			send_to_ics("accept\n");
			break;
		case GTK_RESPONSE_REJECT:
			send_to_ics("decline\n");
			break;
		default:
			debug("got %d response but what does it mean?\n", response_id);
	}
	gtk_widget_destroy (pWidget);
	return TRUE;
}

static gboolean handle_join_channel_response(GtkWidget *dialog, gint response_id, GtkWidget *pWidget) {
	switch(response_id) {
		case GTK_RESPONSE_ACCEPT: {
			GtkComboBoxText *combo_entry = GTK_COMBO_BOX_TEXT(pWidget);
			int num = (int) strtol(gtk_combo_box_text_get_active_text(combo_entry), NULL, 10);
			debug("Requesting join channel: +chan %d\n", num);
			char *command = calloc(128, sizeof(char));
			snprintf(command, 128, "+chan %d\n", num);
			send_to_ics(command);
			free(command);
			break;
		}
		case GTK_RESPONSE_REJECT:
			break;
		default:
			break;
	}
	gtk_widget_destroy(dialog);
	return TRUE;
}

void popup_join_channel_dialog(bool lock_threads) {

	if (lock_threads) {
		gdk_threads_enter();
	}

	GtkWidget *join_channel_dialog = gtk_dialog_new();
	gtk_window_set_modal(GTK_WINDOW(join_channel_dialog), FALSE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW (join_channel_dialog), TRUE);
	gtk_window_set_title(GTK_WINDOW(join_channel_dialog), "Join a new channel");
	gtk_window_set_transient_for(GTK_WINDOW(join_channel_dialog), GTK_WINDOW(main_window));


	GtkWidget *login_button = gtk_dialog_add_button(GTK_DIALOG(join_channel_dialog), "Join", GTK_RESPONSE_ACCEPT);
	gtk_button_set_image(GTK_BUTTON(login_button), gtk_image_new_from_stock(GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON));
	GtkWidget *decline_button = gtk_dialog_add_button(GTK_DIALOG(join_channel_dialog), "Cancel", GTK_RESPONSE_REJECT);
	gtk_button_set_image(GTK_BUTTON(decline_button), gtk_image_new_from_stock(GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON));

	GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG (join_channel_dialog));

	/* Create a radio button with a GtkEntry widget */
	GtkWidget *combo_entry = gtk_combo_box_text_new_with_entry();
	gtk_widget_set_size_request(combo_entry, 300, -1);
	int i;
	for (i=0; i<101; i++) {
		if (!is_in_my_channels(i)) {
			char str[256];
			if (strcmp("", channel_descriptions[i])) {
				sprintf(str, "%d: %s", i, channel_descriptions[i]);
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_entry), NULL, str);
			}

		}
	}

	gtk_box_pack_start(GTK_BOX (content_area), combo_entry, TRUE, TRUE, 2);
	gtk_widget_show_all(join_channel_dialog);

	g_signal_connect(join_channel_dialog, "response", G_CALLBACK(handle_join_channel_response), combo_entry);

	if (lock_threads) {
		gdk_threads_leave();
	}
}

void popup_accept_decline_box(const char *title, const char *message, gboolean lock_threads) {

	if (lock_threads) {
		gdk_threads_enter();
	}

	GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
	                                            GTK_DIALOG_DESTROY_WITH_PARENT,
	                                            GTK_MESSAGE_QUESTION,
	                                            GTK_BUTTONS_NONE,
	                                            "%s", title);

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message);
	GtkWidget *accept_button = gtk_dialog_add_button(GTK_DIALOG(dialog), "Accept", GTK_RESPONSE_ACCEPT);
	gtk_button_set_image(GTK_BUTTON(accept_button), gtk_image_new_from_icon_name("gtk-apply", GTK_ICON_SIZE_BUTTON));
	GtkWidget *decline_button = gtk_dialog_add_button(GTK_DIALOG(dialog), "Decline", GTK_RESPONSE_REJECT);
	gtk_button_set_image(GTK_BUTTON(decline_button), gtk_image_new_from_icon_name("gtk-cancel", GTK_ICON_SIZE_BUTTON));
	/* Send accept or decline to ics or ignore when the user responds to it */
	g_signal_connect_swapped (dialog, "response",
	                          G_CALLBACK (handle_accept_decline_response),
	                          dialog);

	gtk_widget_show(GTK_WIDGET(dialog));

	if (lock_threads) {
		gdk_threads_leave();
	}

}

static int echo_is_off = 0;
void toggle_echo(int on_off) {
	int ret = 0;
	if (on_off) {
		ret = system("stty echo");
	} else {
		ret = system("stty -echo");
		echo_is_off = 1;
	}
	if (ret < 0) {
		fprintf(stderr, "Error while making system call!\n");
	}
}

int scan_append_ply(char *ply) {
	san_scanner__scan_string(ply);
	if (san_scanner_lex() != -1) {
		playing = 1;
		int resolved = resolve_move(main_game, type, currentMoveString, resolved_move);
		if (resolved) {
			char san_move[SAN_MOVE_SIZE];
			int move_result = move_piece(main_game->squares[resolved_move[0]][resolved_move[1]].piece, resolved_move[2], resolved_move[3], 0, AUTO_SOURCE_NO_ANIM, san_move, main_game, false);

			char uci_move[6];
			uci_move[0] = (char) (resolved_move[0] + 'a');
			uci_move[1] = (char) (resolved_move[1] + '1');
			uci_move[2] = (char) (resolved_move[2] + 'a');
			uci_move[3] = (char) (resolved_move[3] + '1');
			if (move_result & PROMOTE) {
				uci_move[4] = (char) (type_to_char(main_game->promo_type) + + 32);
			} else {
				uci_move[4] = '\0';
			}
			user_move_to_uci(uci_move, false);

			append_san_move(main_game, san_move);
			update_eco_tag(true);
			if (is_king_checked(main_game, main_game->whose_turn)) {
				if (is_check_mate(main_game)) {
					san_move[strlen(san_move)] = '#';
				} else {
					san_move[strlen(san_move)] = '+';
				}
			}
			plys_list_append_ply(main_list, ply_new(resolved_move[0], resolved_move[1], resolved_move[2], resolved_move[3], NULL, san_move));
		} else {
			fprintf(stderr, "Could not resolve move %c%s\n", type_to_char(type), currentMoveString);
		}
	} else {
		fprintf(stderr, "san_scanner_lex returned -1\n");
	}
	return FALSE;
}

/* e.g.
  1.  Nf3     (0:00)
*/
int parse_move_list_white_ply(char *message) {
	char w_ply[SAN_MOVE_SIZE];
	memset(w_ply, 0, SAN_MOVE_SIZE);
	char *first_space;
	strtol(message, &first_space, 10);
	first_space++;
	while (*first_space == ' ') first_space++;
	char *second_space = strchr(first_space, ' ');
	if (second_space) {
		memcpy(w_ply, first_space, second_space-first_space);
		scan_append_ply(w_ply);
	}
	return 0;
}

/* e.g.
  4.  exd5    (0:00)     exd5    (0:05)
*/
int parse_move_list_full_move(char *message) {

	char w_ply[SAN_MOVE_SIZE];
	char b_ply[SAN_MOVE_SIZE];
	memset(w_ply, 0, SAN_MOVE_SIZE);
	memset(b_ply, 0, SAN_MOVE_SIZE);

	// parse move number, this will set first space to the dot
	char *first_space;
	strtol(message, &first_space, 10);

	// set first space to the actual first space
	first_space++;

	// inc first_space till first non-space character
	while (*first_space == ' ') first_space++;

	char *second_space = strchr(first_space, ' ');
	if (second_space) {
		memcpy(w_ply, first_space, second_space-first_space);
		scan_append_ply(w_ply);
	}

	// inc second_space till first non-space character
	while (*second_space == ' ') second_space++;
	char *third_space = strchr(second_space, ' ');
	while (*third_space == ' ') third_space++;
	char *fourth_space = strchr(third_space, ' ');
	if (fourth_space) {
		memcpy(b_ply, third_space, fourth_space-third_space);
		scan_append_ply(b_ply);
	}
	return 0;
}

gboolean is_printable(char c) {
	if (c > 31) {
		return TRUE;
	}
	if (c == '\n' || c == '\t') {
		return TRUE;
	}
	return FALSE;
}

void parse_ics_buffer(void) {

	static int chopped_len = 0;

	int i,j;
	char raw_buff[ICS_BUFF_SIZE];


	static char *buff;
	static int current_buff_alloc = 0;

	memset(raw_buff, 0, ICS_BUFF_SIZE);

	// read at most ICS_BUFF_SIZE bytes from the ICS pipe
	int nread = read(ics_data_pipe[0], &raw_buff, ICS_BUFF_SIZE);
	if (nread < 1) {
		fprintf(stderr, "ERROR: failed to read data from ICS pipe\n");
	}

	// Uncomment following block to generate a log of the raw FICS output
	/*
	char log_buff[ICS_BUFF_SIZE];
	memset(log_buff, 0, ICS_BUFF_SIZE);
	memcpy(log_buff, raw_buff, nread);
	FILE *log_file = fopen("log_cairo.txt", "a+");
	fprintf(log_file,"%s", log_buff);
	fclose(log_file);
	*/

	/* *
	 * Check if the case where a truncated line starts our buffer
	 * actually happens (should never happen with a buffer size greater than 512)
	 * */
	if (raw_buff[0] == '\\') {
		debug("Raw Buffer started with a truncated line!!\n");
	}

	/* adjust the memory allocated to buff to match just what we need */
	int new_alloc = nread + chopped_len +1; // allocate 1 extra to NULL terminate
	if (current_buff_alloc != new_alloc) {
		/* realloc might move the pointer so use a temp address */
		char *temp = realloc(buff-chopped_len, new_alloc);
		if (!temp) {
			perror("Realloc failed!!");
			exit(1);
		}
		/* assign the right address to buff */
		buff = temp+chopped_len;
		current_buff_alloc = new_alloc;
	}


	/* memset buff to 0 without overwriting the chopped bit that is before */
	memset(buff, 0, nread+1);


	/* Remove NULLs and \r which confuse the hell out of flex.
	 * If some variable is set, FICS will also send 0x7 (bell) characters
	 * to notifiy of a move, we filter that out here as well */
	j = 0;
	for (i = 0; i < nread; i++) {
		if ( raw_buff[i] != 0 && raw_buff[i] != '\r' && raw_buff[i] != 0x7 ) {
			buff[j] = raw_buff[i];
			j++;
		}
	}
	/* NB: buff is still NULL terminated */

	/////////////
	/* Remove line truncation marks and stitch them */
	/* NB: buff is still NULL terminated */
	/////////////

	if (chopped_len) {
		buff -= chopped_len;
		chopped_len = 0;
	}

	/* Detect a chopped line if the last character in the buffer
	 * is not a \n.
	 * If a chopped line is detected, chopped_len is set appropriately
	 * and we copy the chopped bit to the next scanned buff*/
	char *buff_end = strchr(buff, '\0') - 1; // first null is end of string
	if (*buff_end != '\n') {
		/* The only exceptions to this are the fics, login and password prompts.
		 * These are valid expected chopped lines which we don't want to reparse */
		char *last_endline = strrchr(buff, '\n'); // get last endline
		if (last_endline == NULL) {
			last_endline = buff-1;
		}
		if (!memcmp(last_endline+1, "fics% ", 6)) {
			//debug("DETECTED FICS PROMPT\n");
		}
		else if (!memcmp(last_endline+1, "login: ", 7)) {
			//debug("DETECTED LOGIN PROMPT\n");
		}
		else if (!memcmp(last_endline+1, "password: ", 10)) {
			//debug("DETECTED PASSWORD PROMPT\n");
		}
		else {
			/* Detected a chopped line */
			chopped_len = buff_end - last_endline;

			/* grab chopped bit */
			char *chopped_bit = malloc(chopped_len);
			memcpy(chopped_bit, last_endline + 1, chopped_len);

			/* slide buff */
			memmove(buff+chopped_len, buff, (buff_end-buff + 1)-chopped_len);
			/* NB: buff is still NULL terminated after slide */

			/* copy the chopped bit back to the beginning of buff */
			memcpy(buff, chopped_bit, chopped_len);
			free(chopped_bit);

			/* move buff pointer to the beginning of the unchopped data
			 * This means we won't parse the chopped bit this time round */
			buff += chopped_len;
		}
	}
	else {
		/* Buffer ended by newline */
		chopped_len = 0;
	}

	char *post_buff = calloc(current_buff_alloc-chopped_len+1, sizeof(char));


	ics_scanner__scan_bytes(buff, current_buff_alloc-chopped_len);
	i = 0;
	while (i > -1) {

		i = ics_scanner_lex();

		switch (i) {
			// Dont print the following
			case BOARD_12:
			case WILL_ECHO:
			case WONT_ECHO:
			case FICS_PROMPT:
				break;
				// Print the following
			case LOGIN_PROMPT:
			case PASSWORD_PROMPT:
			case GOT_LOGIN:
			case CREATE_MESSAGE:
			case GAME_START:
			case GAME_END:
			case FOLLOWING:
			default:
				strcat(post_buff, ics_scanner_text);
				break;
		}

		switch (i) {
			case BOARD_12:
				debug("DEBUG got board12\n");
				if (parse_board12(ics_scanner_text+5) > 0) {
					fprintf(stderr, "Failed to parse board12: '%s'\n", ics_scanner_text);
				}
				break;
			case FICS_PROMPT:
				if (my_channels_requested && got_my_channels_header) {
					my_channels_requested = FALSE;
					got_my_channels_header = FALSE;
					sort_my_channels();
					int count_them = count_my_channels();
					if (count_them == my_channels_number) {
						debug("Found my %d channels\n", count_them);
					}
					else {
						fprintf(stderr, "Bug in getting my channels code - Expected to get %d channels but got %d\n", my_channels_number, count_them);
					}
					// No, don't show my channels, Babas does that but that's annoying
					// make it an option on users request
//					show_my_channels();
					show_one_channel(50);
				}
				break;
			case CHANNEL_CHAT: {
				parser_state = CAPTURING_CHAT;
				last_message = calloc(ics_scanner_leng + 1, sizeof(char));
				memset(last_user, 0, 32);
				last_channel_number = parse_channel_chat(ics_scanner_text, last_user, last_message);
				insert_text_channel_view(last_channel_number, last_user, last_message, TRUE);
				free(last_message);
				break;
			}
			case PRIVATE_TELL: {
				parser_state = CAPTURING_CHAT;
				last_message = calloc(ics_scanner_leng + 1, sizeof(char));
				memset(last_user, 0, 32);
				parse_private_tell(ics_scanner_text, last_user, last_message);
				insert_text_channel_view(last_channel_number, last_user, last_message, TRUE);
				free(last_message);
				break;
			}
			case LOGIN_PROMPT:
				debug("DEBUG prompted for login\n");
				ics_logged_in = false;
				show_login_dialog(true);
				break;
			case CONFIRM_GUEST_LOGIN_PROMPT:
				debug("DEBUG prompted for confirming login\n");
				if (guest_mode) {
					send_to_ics("\n");
				}
				break;
			case PASSWORD_PROMPT:
				debug("DEBUG prompted for password\n");
				if (*my_password) {
					send_to_ics(my_password);
				}
				break;
			case INVALID_PASSWORD:
				invalid_password = TRUE;
				break;
			case GOT_LOGIN:
				/* successful login! */
				ics_logged_in = true;
				gdk_threads_enter();
				if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(save_login))) {
					set_save_login(TRUE);
					set_login(gtk_entry_get_text(GTK_ENTRY(login)));
					set_password(gtk_entry_get_text(GTK_ENTRY(password)));
					if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(auto_login))) {
						set_auto_login(TRUE);
					}
					else {
						set_auto_login(FALSE);
					}
				}
				else {
					set_save_login(FALSE);
					set_auto_login(FALSE);
				}

				close_login_dialog(false);

				gdk_threads_leave();

				save_config();

				memset(my_handle, 0, sizeof(my_handle));
				strcpy(my_handle, ics_scanner_text);
				debug("DEBUG got handle for this session: '%s'\n", my_handle);

				set_fics_variables();
				request_my_channels();

				break;

			case CREATE_MESSAGE: {
				gboolean rated;
				if (parse_create_message(ics_scanner_text, &rated, &init_time, &increment) == 5) {
					debug("Successfully parsed create message: player names and ratings\n");
					debug("%s : %s - %s : %s, rated? %d - init: %d, inc: %d\n", current_players[0], current_ratings[0], current_players[1], current_ratings[1], rated, init_time, increment);
					requested_start = 1;
				}
				break;
			}
			case CHALLENGE: {
				char w_name[128];
				char b_name[128];
				char w_rating[5];
				char b_rating[5];
				gboolean rated;
				int basetime;
				int inc;
				int color_specified = parse_challenge_message(ics_scanner_text, w_name, b_name, w_rating, b_rating, &rated, &basetime, &inc);

				debug("Parsed challenge message: player names and ratings\n");
				debug("%s (%s) challenges %s (%s) you for %s game with the following time controls: basetime %d, increment: %d\n",
				      w_name, w_rating, b_name, b_rating, (rated?"a rated":"an unrated"), basetime, inc);
				//											debug("%s", message);
				char *challenger_name = NULL;
				char *challenger_rating = NULL;
				if (!strncmp(my_handle, b_name, 128)) {
					challenger_name = w_name;
					challenger_rating = w_rating;
				}
				else if (!strncmp(my_handle, w_name, 128)) {
					challenger_name = b_name;
					challenger_rating = b_rating;
				}
				if (challenger_name) {
					char message[1024];
					char title[1024];
					snprintf(title, 512, "Challenge: %s (%s) - %s %d %d%s\n", challenger_name, challenger_rating, (rated?"rated":"unrated"), basetime, inc, (color_specified?(color_specified==1?" [Black]":" [White]"):""));
					snprintf(message, 1024, "%s (%s) challenges you for %s game with the following time controls: basetime %d, increment: %d%s\n",
					         challenger_name, challenger_rating, (rated?"a rated":"an unrated"), basetime, inc,(color_specified?(color_specified==1?".\n(You will play White)":".\n(You will play Black)"):"."));
					debug("%s", message);

					popup_accept_decline_box(title, message, TRUE);
				}
				break;
			}
			case FOLLOWING: {
				// You will now be following VMM's games.
				memset(following_player, 0, sizeof(following_player));
				char *bi = strchr(ics_scanner_text, 'g') + 2;
				char *ei = strrchr(ics_scanner_text, '\'');
				memcpy(following_player, bi, ei - bi);
				break;
			}
			case GAME_START: {
				char wn[128], bn[128];
				memset(wn, 0, 128);
				memset(bn, 0, 128);
				long game_num;
				if (parse_start_message(ics_scanner_text, &game_num, wn, bn) == 3) {
					debug("Successfully parsed start message: game number and white/black name\n");

					/* decide whether I am interested in this message
					 * NB: this should always be the case */
					if (strcmp(wn, my_handle) && strcmp(bn, my_handle)) {
						debug("Skipping GAME_START info about game %ld\n", game_num);
						break;
					}

					my_game = game_num;

					memset(main_game->white_name, 0, sizeof(main_game->white_name));
					memset(main_game->black_name, 0, sizeof(main_game->black_name));

					/* determine who's who to assign ratings and build complete strings */
					if (!strcmp(wn, current_players[0])) {
						sprintf(main_game->white_name, "%s (%s)", current_players[0], current_ratings[0]);
						if (!strcmp(bn, current_players[1])) {
							sprintf(main_game->black_name, "%s (%s)", current_players[1], current_ratings[1]);
						}
					}
					else if (!strcmp(wn, current_players[1])) {
						sprintf(main_game->white_name, "%s (%s)", current_players[1], current_ratings[1]);
						if (!strcmp(bn, current_players[0])) {
							sprintf(main_game->black_name, "%s (%s)", current_players[0], current_ratings[0]);
						}
					}
					if (requested_start) {
						// determine relation
						int relation = 1;
						if (!strcmp(bn, my_handle)) relation = -1;
						start_game(main_game->white_name, main_game->black_name, init_time * 60, increment, relation, true);
						start_new_uci_game(init_time * 60, ENGINE_ANALYSIS);
						start_uci_analysis();
						requested_start = 0;
//						if (crafty_mode) {
//							write_to_crafty("new\n");
//							write_to_crafty("log off\n");
//							char crafty_command[256];
//							sprintf(crafty_command, "level 0 %d %d\n", init_time, increment);
//							write_to_crafty(crafty_command);
//							if (relation == 1) {
//								usleep(1000000);
//								write_to_crafty("go\n");
//							}
//						}
					}


				}
				break;
			}
			case GAME_END: {
				char end_token[32];
				if (parse_end_message(ics_scanner_text, end_token) == 4) {

					char bufstr[33];
					if (!main_game->whose_turn) {
						snprintf(bufstr, 33, "\t%s", end_token);
					}
					else {
						strncpy(bufstr, end_token, 32);
					}
					insert_text_moves_list_view(bufstr, true);
					end_game();
				}
//				if (crafty_mode) {
//					write_to_crafty("force\n");
//					if (!crafty_first_guest) {
//						sleep(1);
//						send_to_ics("match cairoguestone 1 0 u\n");
//					}
//				}
				break;
			}
			case WILL_ECHO:
				toggle_echo(0);
				break;
			case WONT_ECHO:
				toggle_echo(1);
				break;
			case OBSERVE_START: {
				long obs_game = parse_observe_start_message(ics_scanner_text);
				debug("Found Observe start for game %ld\n", obs_game);
				if (my_game && my_game != obs_game) {
					debug("my_game != obs_game!!\n");
				}
				my_game = obs_game;
				break;
			}
			case OBSERVE_HEADER: {
				debug("Found Observe Header: '%s'\n", ics_scanner_text);
				long game_num;
				char w_name[128];
				char b_name[128];
				char w_rating[5];
				char b_rating[5];
				memset(w_name, 0, 128);
				memset(b_name, 0, 128);
				memset(w_rating, 0, 5);
				memset(b_rating, 0, 5);
				gboolean rated;
				parse_observe_header(ics_scanner_text, &game_num, w_name, b_name, w_rating, b_rating, &rated, &init_time, &increment);
				if (my_game != game_num) {
					debug("my_game != game_num!!\n");
				}
				char name1[256];
				char name2[256];
				memset(name1, 0, 256);
				memset(name2, 0, 256);
				sprintf(name1, "%s (%s)", w_name, w_rating);
				sprintf(name2, "%s (%s)", b_name, b_rating);
				start_game(name1, name2, init_time * 60, increment, 0, true);
				start_new_uci_game(init_time * 60, ENGINE_ANALYSIS);
				if (!strcmp(b_name, following_player) && !is_board_flipped() || !strcmp(w_name, following_player) && is_board_flipped()) {
					g_signal_emit_by_name(board, "flip-board");
				}
				requested_times = 1;
				char request_moves[16];
				snprintf(request_moves, 16, "moves %ld\n", game_num);
				requested_moves = 1;
				finished_parsing_moves = 0;
				send_to_ics(request_moves);
				break;
			}
			case MOVE_LIST_START:
				debug("Found Movelist start: '%s'\n", ics_scanner_text);
				if (!requested_moves) {
					debug("Found Movelist start but we didn't ask for any moves?: '%s'\n", ics_scanner_text);
				} else {
					parsed_plys = 0;
					got_header = 1;
				}
				break;
			case MOVE_LIST_WHITE_PLY:
				if (requested_moves && got_header) {
					parsed_plys++;
					parse_move_list_white_ply(ics_scanner_text);
				}
				break;
			case MOVE_LIST_FULL_MOVE:
				if (requested_moves && got_header) {
					parsed_plys += 2;
					parse_move_list_full_move(ics_scanner_text);
				}
				break;
			case MOVE_LIST_END:
				got_header = 0;
				requested_moves = 0;
				finished_parsing_moves = 1;

				refresh_moves_list_view(main_list);
				gdk_threads_enter();
				draw_pieces_surface(old_wi, old_hi);
				init_dragging_background(old_wi, old_hi);
				init_highlight_under_surface(old_wi, old_hi);
//				init_highlight_over_surface(old_wi, old_hi);

				// highlight last move
				if (parsed_plys > 0 && highlight_last_move) {
					highlight_move(resolved_move[0], resolved_move[1], resolved_move[2], resolved_move[3], old_wi, old_hi);
				}
				if (parsed_plys > 0 && is_king_checked(main_game, main_game->whose_turn)) {
					warn_check(old_wi, old_hi);
				}

				gtk_widget_queue_draw(GTK_WIDGET(board));
				gdk_threads_leave();
				if (parsed_plys > 1 && !clock_started) {
					clock_started = 1;
					start_one_clock(main_clock, (main_game->whose_turn));
				}
				start_uci_analysis();
				break;
			case GAME_RESUME: {
				debug("Found GAME_RESUME message: '%s'\n", ics_scanner_text);
				char wn[128], bn[128];
				memset(wn, 0, 128);
				memset(bn, 0, 128);
				long game_num;
				if (parse_resume_message(ics_scanner_text, &game_num, wn, bn) == 3) {
					debug("Successfully parsed start message: game number and white/black name\n");
					my_game = game_num;
				}

				/////
				/* determine who's who to assign ratings and build complete strings */
				if (!strcmp(wn, current_players[0])) {
					sprintf(main_game->white_name, "%s (%s)", current_players[0], current_ratings[0]);
					if (!strcmp(bn, current_players[1])) {
						sprintf(main_game->black_name, "%s (%s)", current_players[1], current_ratings[1]);
					}
				}
				else if (!strcmp(wn, current_players[1])) {
					sprintf(main_game->white_name, "%s (%s)", current_players[1], current_ratings[1]);
					if (!strcmp(bn, current_players[0])) {
						sprintf(main_game->black_name, "%s (%s)", current_players[0], current_ratings[0]);
					}
				}
				if (requested_start) {
					// determine relation
					int relation = 1;
					if (!strcmp(bn, my_handle)) relation = -1;
					start_game(main_game->white_name, main_game->black_name, init_time*60, increment, relation, true);
					requested_start = 0;
				}
				/////
				requested_times = 1;
				char request_moves[16];
				snprintf(request_moves, 16, "moves %ld\n", game_num);
				requested_moves = 1;
				finished_parsing_moves = 0;
				send_to_ics(request_moves);
				break;
			}
			case MY_CHANNELS_HEADER:
				if (my_channels_requested) {
					my_channels_number = parse_my_channels_header(ics_scanner_text);
					got_my_channels_header = TRUE;
				}
				break;
			case MY_CHANNELS_LINE:
				if (my_channels_requested) {
					parse_my_channels_line(ics_scanner_text);
				}
				break;
			case CHANNEL_REMOVED: {
				int removed_channel = strtol(ics_scanner_text+1, NULL, 10);
				handle_channel_removed(removed_channel);
				break;
			}
			case CHANNEL_ADDED: {
				int added_channel = strtol(ics_scanner_text+1, NULL, 10);
				handle_channel_added(added_channel);
				break;
			}
			default:
				break;
		}
	}

	if (strlen(post_buff) != 1 || post_buff[0] != '\n') {
		fprintf(stdout, "%s", post_buff);
		fflush(stdout);
	}
	free(post_buff);


	return;
}

void *read_message_function(void *ptr) {
	int *socket = (int *) (ptr);

	while (!read_write_ics_fd(STDIN_FILENO, ics_data_pipe[1], *socket)) {
		usleep(10000);
	}

	fprintf(stdout, "[read ics thread] - Closing ICS reader\n");
	return 0;
}

void *parse_ics_function(void *ptr) {

	while (is_running_flag()) {
		parse_ics_buffer();
	}

	fprintf(stdout, "[parse ics thread] - Closing ICS parser\n");
	return 0;
}

int init_ics() {
	ics_fd = open_tcp(ics_host, ics_port);
	if (ics_fd < 0) {
		fprintf(stderr, "Error connecting!\n");
		return 1;
	}
	fprintf(stdout, "Connected to ICS server.\n");
	if (pipe(ics_data_pipe)) {
		perror("Pipe creation failed");
		return 1;
	}
	pthread_create(&ics_reader_thread, NULL, read_message_function, (void*)(&ics_fd));
	pthread_create(&ics_buff_parser_thread, NULL, parse_ics_function, (void*)(&ics_fd));
	return 0;
}

void cleanup_ics() {
	pthread_cancel(ics_reader_thread);
	pthread_cancel(ics_buff_parser_thread);
	pthread_join(ics_reader_thread, NULL);
	pthread_join(ics_buff_parser_thread, NULL);
	if (echo_is_off) {
		toggle_echo(1);
	}
}
