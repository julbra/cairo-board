#include <gtk/gtk.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include "chess-backend.h"
#include "analysis_panel.h"
#include "uci-adapter.h"
#include "uci_scanner.h"

static int uci_in;
static int uci_out;
static int uci_err;

static pthread_t uci_read_thread;

static regex_t option_matcher;
static regex_t best_move_ponder_matcher;
static regex_t best_move_matcher;
static regex_t info_depth_matcher;
static regex_t info_selective_depth_matcher;
static regex_t info_time_matcher;
static regex_t info_score_cp_matcher;
static regex_t info_score_mate_matcher;
static regex_t info_nps_matcher;
static regex_t info_best_line_matcher;

static char engine_name[256] = "";
static bool uci_ok = false;
static bool uci_ready = false;
static unsigned int plyNum;
static int toPlay;
static bool pondering;

int uci_scanner__scan_bytes(const char *, int length);
void *parse_uci_function();
void wait_for_engine(void);
void user_move_to_uci(char *move);

void write_to_uci(char *message) {
	if (write(uci_in, message, strlen(message)) == -1) {
		perror(NULL);
	}
	debug("Wrote to UCI: %s", message);
}

void wait_for_engine(void) {
	uci_ready = false;
	write_to_uci("isready\n");
	while(!uci_ready) {
		usleep(500);
	}
}

void compile_regex(regex_t* regex, const char *pattern) {
	int status = regcomp(regex, pattern, REG_EXTENDED);
	if (status != 0) {
		char err_buf[BUFSIZ];
		regerror(status, regex, err_buf, BUFSIZ);
		fprintf(stderr, "init_regex(): regcomp failed - %s\n", err_buf);
		exit(1);
	}
}

void init_regex() {
	compile_regex(&option_matcher, "option name (.*) type (.*)");
	compile_regex(&best_move_ponder_matcher, "bestmove (.*) ponder (.*)");
	compile_regex(&best_move_matcher, "bestmove (.*)");

	// Info matchers
	compile_regex(&info_depth_matcher, "depth ([0-9]+)");
	compile_regex(&info_selective_depth_matcher, "seldepth ([0-9]+)");
	compile_regex(&info_time_matcher, "time ([0-9]+)");
	compile_regex(&info_score_cp_matcher, "score cp (-?[0-9]+)");
	compile_regex(&info_score_mate_matcher, "score mate (-?[0-9]+)");
	compile_regex(&info_nps_matcher, "nps ([0-9]+)");
	compile_regex(&info_best_line_matcher, " pv ([a-h1-8 ]+)");
}

int spawn_uci_engine(void) {
	GPid child_pid;

	int i;
	GError *spawnError = NULL;

	gchar *argv[] = {"/usr/bin/stockfish", NULL};

	gboolean ret = g_spawn_async_with_pipes(g_get_home_dir(), argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, &child_pid,
	                                        &uci_in, &uci_out, &uci_err, &spawnError);

	if (!ret) {
		g_error("spawn_uci_engine FAILED %s", spawnError->message);
		return -1;
	}

	init_regex();

	pthread_create(&uci_read_thread, NULL, parse_uci_function, (void *) (&i));
	write_to_uci("uci\n");
	while (!uci_ok) {
		usleep(500);
	}
	printf("UCI OK!\n");

	write_to_uci("setoption name Threads value 1\n");
	write_to_uci("setoption name Hash value 512\n");
	write_to_uci("setoption name Ponder value true\n");
	write_to_uci("setoption name Skill Level value 0\n");
	wait_for_engine();
	return 0;
}

static char allMoves[8192];

void start_new_uci_game(int time) {
	debug("Start UCI game\n");
	write_to_uci("ucinewgame\n");
	wait_for_engine();
	debug("Before start_game\n");
	start_game("You", engine_name, time, 0, 1, false);
	debug("After start_game\n");
	memcpy(allMoves, "position startpos moves", 24);
	plyNum = 1;
	toPlay = 0;
	pondering = false;
}

void extract_match(char *src, regmatch_t aMatch, char *dest) {
	unsigned int len = (unsigned int) (aMatch.rm_eo - aMatch.rm_so);
	memcpy(dest, src + aMatch.rm_so, len);
	dest[len] = '\0';
}

void append_move(char *newMove, bool lock_threads) {
	debug("append_move: %s\n", newMove);
	size_t newMoveLen = strlen(newMove);
	size_t movesLength = strlen(allMoves);
	allMoves[movesLength] = ' ';
	memcpy(allMoves + movesLength + 1, newMove, newMoveLen + 1); // includes terminating null
	toPlay = toPlay ? 0 : 1;

	debug("append_move: allMoves '%s'\n", allMoves);
	if (plyNum == 1) {
		start_one_clock(main_clock, toPlay);
	} else if (plyNum > 1) {
		start_one_stop_other_clock(main_clock, toPlay, lock_threads);
	}

	plyNum++;
}

void user_move_to_uci(char *move) {
	debug("User move to UCI! '%s'\n", move);

	// Append move
	debug("append_move go %s\n", move);
	append_move(move, false);

	char moves[8192];
	sprintf(moves, "%s\n", allMoves);
	debug("moves %s\n", moves);

	if (pondering) {
		write_to_uci("stop\n");
	}
	wait_for_engine();

	write_to_uci(moves);
	char go[256];
	sprintf(go, "go wtime %ld btime %ld\n", get_remaining_time(main_clock, 0), get_remaining_time(main_clock, 1));
	debug("sending go %s\n", go);
	write_to_uci(go);
}

void parse_option(char *optionText) {
	regmatch_t pmatch[3];
	int status = regexec(&option_matcher, optionText, 3, pmatch, 0);
	if (!status) {
		char optName[256];
		char optType[256
		];
		extract_match(optionText, pmatch[1], optName);
		extract_match(optionText, pmatch[2], optType);
		debug("Option %s, type: %s\n", optName, optType);
	} else if (status == REG_NOMATCH) {
		// Would only happen if engine is broken
		debug("No match");
	}
}

void parse_move(char *moveText) {
	/* Note: UCI uses a weird notation, unlike what the spec says it is not the *long algebraic notation* (LAN) at all...
	 For example: a Knight to f3 move LAN is Ng1-f3 but we get g1f3
	 Also, promotions are indicated like such: a7a8q*/

	if (pondering) {
		debug("Skip pondering best move: %s\n", moveText);
		pondering = false;
		return;
	}

	regmatch_t pmatch[2];
	int status = regexec(&best_move_matcher, moveText, 2, pmatch, 0);
	char bestMove[6];
	if (!status) {
		extract_match(moveText, pmatch[1], bestMove);
		debug("Got UCI best move with ponder: %s\n", bestMove);
	} else if (status == REG_NOMATCH) {
		// Would only happen if engine is broken
		debug("No match");
	}

	size_t bestMoveLen = strlen(bestMove);

	if (bestMoveLen > 4) {
		debug("Handling promotion from Engine '%s' promo: '%c'\n", bestMove, bestMove[4]);
		promo_type = char_to_type((char) (bestMove[4] - 32));
		debug("Handling promotion from Engine %c -> %d\n", bestMove[4], promo_type);
	}
	// Append move
	append_move(bestMove, true);

	set_last_move(bestMove);
	g_signal_emit_by_name(board, "got-uci-move");
}

void parse_move_with_ponder(char *moveText) {

	/* Note: UCI uses a weird notation, unlike what the spec says it is not the *long algebraic notation* (LAN) at all...
	 For example, for a Knight to f3 move, LAN would be Ng1-f3
	 Instead we get g1f3 */

	if (pondering) {
		debug("Skip pondering best move: %s\n", moveText);
		pondering = false;
		return;
	}

	regmatch_t pmatch[3];
	int status = regexec(&best_move_ponder_matcher, moveText, 3, pmatch, 0);
	char bestMove[6];
	char ponderMove[6];
	if (!status) {
		extract_match(moveText, pmatch[1], bestMove);
		extract_match(moveText, pmatch[2], ponderMove);
		debug("Got UCI best move with ponder: %s; Ponder: %s\n", bestMove, ponderMove);
	} else if (status == REG_NOMATCH) {
		// Would only happen if engine is broken
		debug("No match");
	}

	size_t bestMoveLen = strlen(bestMove);

	if (bestMoveLen > 4) {
		promo_type = char_to_type((char) (bestMove[4] - 32));
		debug("Handling promotion from Engine %c -> %d\n", bestMove[4], promo_type);
	}

	// Append move
	append_move(bestMove, true);

	set_last_move(bestMove);
	g_signal_emit_by_name(board, "got-uci-move");

	char moves[8192];
	sprintf(moves, "%s %s\n", allMoves, ponderMove);
	write_to_uci(moves);
	write_to_uci("go ponder\n");
	pondering = true;
}

void parse_info(char *infoDepth) {
	regmatch_t pmatch[2];
	int status;

	char scoreValue[16];
	char scoreString[16];
	status = regexec(&info_score_cp_matcher, infoDepth, 2, pmatch, 0);
	if (!status) {
		extract_match(infoDepth, pmatch[1], scoreValue);
		int scoreInt = (int) strtol(scoreValue, NULL, 10);
		snprintf(scoreString, 16, "%.2f", scoreInt / 100.0);
		debug("Got UCI info score: cp -> %s\n", scoreString);
		set_analysis_score(scoreString);
	} else {
		status = regexec(&info_score_mate_matcher, infoDepth, 2, pmatch, 0);
		if (!status) {
			extract_match(infoDepth, pmatch[1], scoreValue);
			snprintf(scoreString, 16, "#%s", scoreValue);
			debug("Got UCI info score: mate -> %s\n", scoreString);
			set_analysis_score(scoreString);
		}
	}

	char bestLine[BUFSIZ];
	memset(bestLine, 0, BUFSIZ);
	status = regexec(&info_best_line_matcher, infoDepth, 2, pmatch, 0);
	if (!status) {
		extract_match(infoDepth, pmatch[1], bestLine);
		debug("Got UCI info best line: pv -> %s\n", bestLine);
		set_analysis_best_line(bestLine);
	}

	char nps[16];
	char npsString[32];
	status = regexec(&info_nps_matcher, infoDepth, 2, pmatch, 0);
	if (!status) {
		extract_match(infoDepth, pmatch[1], nps);
		int npsInt = (int) strtol(nps, NULL, 10);
		snprintf(npsString, 32, "%d kNps", npsInt / 1000);
		set_analysis_nodes_per_second(npsString);
	}
}

void parse_uci_buffer(void) {

	char raw_buff[BUFSIZ];

	memset(raw_buff, 0, BUFSIZ);
	int nread = (int) read(uci_out, &raw_buff, BUFSIZ);
	if (nread < 1) {
		fprintf(stderr, "ERROR: failed to read data from UCI Engine pipe\n");
        usleep(1000000);
        return;
	}
	uci_scanner__scan_bytes(raw_buff, nread);
	int i = 0;
	while (i > -1) {
		i = uci_scanner_lex();
		switch (i) {
			case UCI_OK: {
				uci_ok = true;
				break;
			}
			case UCI_READY: {
				uci_ready = true;
				break;
			}
			case UCI_ID_NAME: {
				printf("Got UCI Name: %s\n", uci_scanner_text);
				strcpy(engine_name, uci_scanner_text + 8);
				break;
			}
			case UCI_ID_AUTHOR: {
				printf("Got UCI Author: %s\n", uci_scanner_text);
				break;
			}
			case UCI_OPTION: {
				parse_option(uci_scanner_text);
				break;
			}
			case UCI_BEST_MOVE_WITH_PONDER: {
				parse_move_with_ponder(uci_scanner_text);
				break;
			}
			case UCI_BEST_MOVE: {
				parse_move(uci_scanner_text);
				break;
			}
			case UCI_INFO: {
				parse_info(uci_scanner_text);
				break;
			}
			case LINE_FEED:
			case EMPTY_LINE:
			default:
				break;
		}
	}
}

void *parse_uci_function() {
	fprintf(stdout, "[parse UCI thread] - Starting UCI parser\n");

	while (g_atomic_int_get(&running_flag)) {
		parse_uci_buffer();
	}

	fprintf(stdout, "[parse UCI thread] - Closing UCI parser\n");
	return 0;
}
