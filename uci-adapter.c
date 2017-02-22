#include <gtk/gtk.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include "chess-backend.h"
#include "uci-adapter.h"
#include "uci_scanner.h"

static int uci_in;
static int uci_out;
static int uci_err;

static pthread_t uci_read_thread;
static regex_t optionMatcher;
static regex_t bestMovePonderMatcher;
static regex_t bestMoveMatcher;

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

void init_regex() {
	int status = regcomp(&optionMatcher, "option name (.*) type (.*)", REG_EXTENDED);
	if (status != 0) {
		char err_buf[BUFSIZ];
		regerror(status, &optionMatcher, err_buf, BUFSIZ);
		fprintf(stderr, "init_regex(): regcomp failed - %s\n", err_buf);
		exit(1);
	}
	status = regcomp(&bestMovePonderMatcher, "bestmove (.*) ponder (.*)", REG_EXTENDED);
	if (status != 0) {
		char err_buf[BUFSIZ];
		regerror(status, &bestMovePonderMatcher, err_buf, BUFSIZ);
		fprintf(stderr, "init_regex(): regcomp failed - %s\n", err_buf);
		exit(1);
	}
	status = regcomp(&bestMoveMatcher, "bestmove (.*)", REG_EXTENDED);
	if (status != 0) {
		char err_buf[BUFSIZ];
		regerror(status, &bestMovePonderMatcher, err_buf, BUFSIZ);
		fprintf(stderr, "init_regex(): regcomp failed - %s\n", err_buf);
		exit(1);
	}
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
	wait_for_engine();
	return 0;
}

static char allMoves[8192];

void startNewUciGame(int time) {
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

void extractMatch(char *src, regmatch_t aMatch, char *dest) {
	unsigned int len = (unsigned int) (aMatch.rm_eo - aMatch.rm_so);
	memcpy(dest, src + aMatch.rm_so, len);
	dest[len] = '\0';
}

void appendMove(char *newMove, bool lock_threads) {
	debug("appendMove: %s\n", newMove);
	size_t newMoveLen = strlen(newMove);
	size_t movesLength = strlen(allMoves);
	allMoves[movesLength] = ' ';
	memcpy(allMoves + movesLength + 1, newMove, newMoveLen + 1); // includes terminating null
	toPlay = toPlay ? 0 : 1;

	debug("appendMove: allMoves '%s'\n", allMoves);
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
	debug("appendMove go %s\n", move);
	appendMove(move, false);

	char moves[8192];
	sprintf(moves, "%s\n", allMoves);
	debug("moves %s\n", moves);

	if (pondering) {
		write_to_uci("stop\n");
	}
	wait_for_engine();

	write_to_uci(moves);
	debug("before go\n");
	char go[256];
	sprintf(go, "go wtime %ld btime %ld\n", get_remaining_time(main_clock, 0), get_remaining_time(main_clock, 1));
	debug("before go %s\n", go);
	write_to_uci(go);
	debug("after go %s\n", go);
}

void parseOption(char *optionText) {
	regmatch_t pmatch[3];
	int status = regexec(&optionMatcher, optionText, 3, pmatch, 0);
	if (!status) {
		char optName[256];
		char optType[256
		];
		extractMatch(optionText, pmatch[1], optName);
		extractMatch(optionText, pmatch[2], optType);
		debug("Option %s, type: %s\n", optName, optType);
	} else if (status == REG_NOMATCH) {
		// Would only happen if engine is broken
		debug("No match");
	}
}

void parseMove(char *moveText) {
	/* Note: UCI uses a weird notation, unlike what the spec says it is not the *long algebraic notation* (LAN) at all...
	 For example, for a Knight to f3 move, LAN would be Ng1-f3
	 Instead we get g1f3 */

	if (pondering) {
		debug("Skip pondering best move: %s\n", moveText);
		pondering = false;
		return;
	}

	regmatch_t pmatch[3];
	int status = regexec(&bestMoveMatcher, moveText, 2, pmatch, 0);
	char bestMove[6];
	if (!status) {
		extractMatch(moveText, pmatch[1], bestMove);
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
	appendMove(bestMove, true);

	set_last_move(bestMove);
	g_signal_emit_by_name(board, "got-uci-move");
}

void parseMoveWithPonder(char *moveText) {

	/* Note: UCI uses a weird notation, unlike what the spec says it is not the *long algebraic notation* (LAN) at all...
	 For example, for a Knight to f3 move, LAN would be Ng1-f3
	 Instead we get g1f3 */

	if (pondering) {
		debug("Skip pondering best move: %s\n", moveText);
		pondering = false;
		return;
	}

	regmatch_t pmatch[3];
	int status = regexec(&bestMovePonderMatcher, moveText, 3, pmatch, 0);
	char bestMove[6];
	char ponderMove[6];
	if (!status) {
		extractMatch(moveText, pmatch[1], bestMove);
		extractMatch(moveText, pmatch[2], ponderMove);
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
	appendMove(bestMove, true);

	set_last_move(bestMove);
	g_signal_emit_by_name(board, "got-uci-move");

	char moves[8192];
	sprintf(moves, "%s %s\n", allMoves, ponderMove);
	write_to_uci(moves);
	write_to_uci("go ponder\n");
	pondering = true;
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
				parseOption(uci_scanner_text);
				break;
			}
			case UCI_BEST_MOVE_WITH_PONDER: {
				parseMoveWithPonder(uci_scanner_text);
				break;
			}
			case UCI_BEST_MOVE: {
				parseMove(uci_scanner_text);
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
