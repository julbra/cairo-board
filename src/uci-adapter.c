#include <gtk/gtk.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include "chess-backend.h"
#include "analysis_panel.h"
#include "uci-adapter.h"
#include "uci_scanner.h"

const static unsigned int STOP_TIMEOUT_SEC = 5;
const static unsigned int READY_TIMEOUT_SEC = 3;

int uci_scanner__scan_bytes(const char *bytes, int len);

static int uci_in;
static int uci_out;
static int uci_err;

static int uci_user_in[2];

static pthread_t uci_read_thread;
static pthread_t uci_manager_thread;

static regex_t option_matcher;
static regex_t best_move_ponder_matcher;
static regex_t best_move_matcher;
static regex_t info_selective_depth_matcher;
static regex_t info_time_matcher;
static regex_t info_score_cp_matcher;
static regex_t info_score_mate_matcher;
static regex_t info_depth_matcher;
static regex_t info_selective_depth_matcher;
static regex_t info_nps_matcher;
static regex_t info_best_line_matcher;

static char engine_name[256] = "";

static bool uci_ok = 0;
static bool uci_ready = 0;
static bool analysing = 0;
static bool stop_requested = 0;

static pthread_mutex_t uci_writer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t uci_ok_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t uci_ready_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t analysing_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t stop_requested_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t all_moves_lock = PTHREAD_MUTEX_INITIALIZER;

static unsigned int ply_num;
static int to_play;

static char all_moves[4 * 8192];
char shown_best_line[BUFSIZ] = "";
size_t shown_best_line_len = 0;
static UCI_MODE uci_mode;
static unsigned int game_time = 0;

bool play_vs_machine;
const char START_SEQUENCE[] = "position startpos moves";

// Commands to UCI manager
const static char START_NEW_GAME_COMMAND[] = "start_new_game\n";
const static char START_NEW_GAME_TOKEN[] = "start_new_game";
const static char START_ANALYSIS_COMMAND[] = "start_analysis\n";
const static char START_ANALYSIS_TOKEN[] = "start_analysis";

// Private prototypes
static void *parse_uci_function(void *);
static void *uci_manager_function(void *);
static void wait_for_engine_ready(void);
static void best_line_to_san(char line[8192], char san[8192]);

void cleanup_uci() {
	pthread_mutex_destroy(&uci_writer_lock);
	pthread_mutex_destroy(&uci_ok_lock);
	pthread_mutex_destroy(&uci_ready_lock);
	pthread_mutex_destroy(&analysing_lock);
	pthread_mutex_destroy(&stop_requested_lock);
	pthread_mutex_destroy(&all_moves_lock);
}

void set_uci_ok(bool val) {
	pthread_mutex_lock(&uci_ok_lock);
	uci_ok = val;
	pthread_mutex_unlock(&uci_ok_lock);
}

bool is_uci_ok() {
	bool val;
	pthread_mutex_lock(&uci_ok_lock);
	val = uci_ok;
	pthread_mutex_unlock(&uci_ok_lock);
	return val;
}

void set_uci_ready(bool val) {
	pthread_mutex_lock(&uci_ready_lock);
	uci_ready = val;
	pthread_mutex_unlock(&uci_ready_lock);
}

bool is_uci_ready() {
	bool val;
	pthread_mutex_lock(&uci_ready_lock);
	val = uci_ready;
	pthread_mutex_unlock(&uci_ready_lock);
	return val;
}

void set_analysing(bool val) {
	pthread_mutex_lock(&analysing_lock);
	analysing = val;
	pthread_mutex_unlock(&analysing_lock);
}

bool is_analysing() {
	bool val;
	pthread_mutex_lock(&analysing_lock);
	val = analysing;
	pthread_mutex_unlock(&analysing_lock);
	return val;
}

void set_stop_requested(bool val) {
	pthread_mutex_lock(&stop_requested_lock);
	stop_requested = val;
	pthread_mutex_unlock(&stop_requested_lock);
}

bool is_stop_requested() {
	bool val;
	pthread_mutex_lock(&stop_requested_lock);
	val = stop_requested;
	pthread_mutex_unlock(&stop_requested_lock);
	return val;
}

void compile_regex(regex_t* regex, const char *pattern) {
	int status = regcomp(regex, pattern, REG_EXTENDED);
	if (status != 0) {
		char err_buf[BUFSIZ];
		regerror(status, regex, err_buf, BUFSIZ);
		fprintf(stderr, "compile_regex(): regcomp failed - %s\n", err_buf);
		exit(1);
	}
}

void init_uci_adapter() {
	compile_regex(&option_matcher, "option name (.*) type (.*)");
	compile_regex(&best_move_ponder_matcher, "bestmove (.*) ponder (.*)");
	compile_regex(&best_move_matcher, "bestmove (.*)");

	// Info matchers
	compile_regex(&info_depth_matcher, " depth ([0-9]+)");
	compile_regex(&info_selective_depth_matcher, " seldepth ([0-9]+)");
	compile_regex(&info_time_matcher, " time ([0-9]+)");
	compile_regex(&info_score_cp_matcher, "score cp (-?[0-9]+)( upperbound| lowerbound)?");
	compile_regex(&info_score_mate_matcher, "score mate (-?[0-9]+)");
	compile_regex(&info_nps_matcher, " nps ([0-9]+)");
	compile_regex(&info_best_line_matcher, " pv ([a-h1-8rnbq ]+)");

	int result = pipe(uci_user_in);
	if (result < 0) {
		perror("Failed to create UCI manager pipe ");
		exit(1);
	}

	pthread_create(&uci_read_thread, NULL, parse_uci_function, NULL);
	pthread_create(&uci_manager_thread, NULL, uci_manager_function, NULL);
}

int spawn_uci_engine(bool brainfish) {
	GPid child_pid;


	GError *spawnError = NULL;

	gchar *argv[2];
	if (brainfish) {
		argv[0] = "/usr/local/bin/brainfish";
	} else {
		argv[0] = "/usr/bin/stockfish";
	}
	argv[1] = NULL;

	gboolean ret = g_spawn_async_with_pipes(g_get_home_dir(), argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, &child_pid,
	                                        &uci_in, &uci_out, &uci_err, &spawnError);

	if (!ret) {
		g_error("spawn_uci_engine FAILED %s", spawnError->message);
		return -1;
	}

	init_uci_adapter();

	write_to_uci("uci\n");
	while (!is_uci_ok()) {
		usleep(500);
	}
	debug("UCI OK!\n");

	write_to_uci("setoption name Threads value 1\n");
	write_to_uci("setoption name Hash value 2048\n");
	write_to_uci("setoption name Ponder value true\n");
	write_to_uci("setoption name Skill Level value 20\n");
	if (brainfish) {
		write_to_uci("setoption name BookPath value /home/hts/brainfish/Cerebellum_Light.bin\n");
	}
	wait_for_engine_ready();
	return 0;
}

void start_new_uci_game(unsigned int initial_time, UCI_MODE mode) {
	debug("Start UCI - game mode: %d\n", mode);

	uci_mode = mode;
	game_time = initial_time;

	pthread_mutex_lock(&all_moves_lock);
	memset(all_moves, 0, 4 * 8192);
	memcpy(all_moves, START_SEQUENCE, 24);
	debug("All moves set to: '%s'\n", all_moves);
	pthread_mutex_unlock(&all_moves_lock);

	ply_num = 1;
	to_play = 0;

	if (write(uci_user_in[1], START_NEW_GAME_COMMAND, sizeof(START_NEW_GAME_COMMAND)) == -1) {
		perror("Failed to start new UCI game via the UCI manager ");
	}
}

void start_uci_analysis() {
	if (write(uci_user_in[1], START_ANALYSIS_COMMAND, sizeof(START_ANALYSIS_COMMAND)) == -1) {
		perror("Failed to start UCI analysis via the UCI manager ");
	}
}

void extract_match(char *src, regmatch_t aMatch, char *dest) {
	unsigned int len = (unsigned int) (aMatch.rm_eo - aMatch.rm_so);
	memcpy(dest, src + aMatch.rm_so, len);
	dest[len] = '\0';
}

void append_move(char *new_move, bool lock_threads) {
	debug("append_move: %s\n", new_move);

	size_t newMoveLen = strlen(new_move);

	pthread_mutex_lock(&all_moves_lock);
	size_t movesLength = strlen(all_moves);
	all_moves[movesLength] = ' ';
	memcpy(all_moves + movesLength + 1, new_move, newMoveLen + 1); // includes terminating null
	pthread_mutex_unlock(&all_moves_lock);

	to_play = to_play ? 0 : 1;

	debug("append_move: all_moves '%s'\n", all_moves);
	if (!ics_mode && !load_file_specified) {
//	if (!ics_mode) {
		if (ply_num == 1) {
			start_one_clock(main_clock, to_play);
		} else if (ply_num > 1) {
			start_one_stop_other_clock(main_clock, to_play, lock_threads);
		}
	}

	ply_num++;
}

void user_move_to_uci(char *move, bool analyse) {
	debug("User move to UCI! '%s'\n", move);

	// Append move
	append_move(move, false);

	if (analyse) {
		start_uci_analysis();
	}
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

bool handle_move_after_stop() {
	if (is_stop_requested() && is_analysing()) {
		debug("Skip final best move after stop\n");
		set_analysing(false);
		set_stop_requested(false);
		return true;
	}
	return false;
}

void parse_move(char *moveText) {

	/* Note: UCI uses a weird notation, unlike what the spec says it is not the *long algebraic notation* (LAN) at all...
	 For example: a Knight to f3 move LAN is Ng1-f3 but we get g1f3
	 Also, promotions are indicated like such: a7a8q*/

	if (handle_move_after_stop()) {
		return;
	}

	set_analysing(false);

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
		main_game->promo_type = char_to_type(main_game->whose_turn, (char) (bestMove[4] - 32));
		debug("Handling promotion from Engine %c -> %d\n", bestMove[4], main_game->promo_type);
	}
	// Append move
	append_move(bestMove, true);

	set_last_move(bestMove);
	g_signal_emit_by_name(board, "got-uci-move");

	char moves[8192];
	pthread_mutex_lock(&all_moves_lock);
	sprintf(moves, "%s\n", all_moves);
	pthread_mutex_unlock(&all_moves_lock);

	write_to_uci(moves);
//	write_to_uci("go ponder\n");
	write_to_uci("go infinite\n");
	set_analysing(true);
}

void parse_move_with_ponder(char *moveText) {

	/* Note: UCI uses a weird notation, unlike what the spec says it is not the *long algebraic notation* (LAN) at all...
	 For example, for a Knight to f3 move, LAN would be Ng1-f3
	 Instead we get g1f3 */

	if (handle_move_after_stop()) {
		return;
	}

	set_analysing(false);

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
		main_game->promo_type = char_to_type(main_game->whose_turn, (char) (bestMove[4] - 32));
		debug("Handling promotion from Engine %c -> %d\n", bestMove[4], main_game->promo_type);
	}

	// Append move
	append_move(bestMove, true);

	set_last_move(bestMove);
	g_signal_emit_by_name(board, "got-uci-move");

	char moves[8192];
	pthread_mutex_lock(&all_moves_lock);
	//	sprintf(moves, "%s %s\n", all_moves, ponderMove);
	sprintf(moves, "%s\n", all_moves);
	pthread_mutex_unlock(&all_moves_lock);

	write_to_uci(moves);
//	write_to_uci("go ponder\n");
	write_to_uci("go infinite\n");
	set_analysing(true);
}

void parse_info(char *info) {

	if (is_stop_requested() && is_analysing()) {
//		debug("Skip info while stopping\n");
		return;
	}

	regmatch_t pmatch[3];
	int status;

	char score_value[16];
	score_value[0] = '\0';
	bool score_is_mate = false;

	char scoreString[16];
	status = regexec(&info_score_cp_matcher, info, 3, pmatch, 0);
	if (!status) {
		extract_match(info, pmatch[1], score_value);
		// Uncomment the following to extract upper or lower bound info
//		if (pmatch[2].rm_so > -1 && pmatch[2].rm_eo > -1) {
//			char score_bound[16];
//			extract_match(info, pmatch[2], score_bound);
//			debug("Score is %s\n", score_bound + 1);
//		}
	} else {
		status = regexec(&info_score_mate_matcher, info, 2, pmatch, 0);
		if (!status) {
			score_is_mate = true;
			extract_match(info, pmatch[1], score_value);
		}
	}

	if (score_value[0] != '\0') {
		int score_int = (int) strtol(score_value, NULL, 10);
		switch (uci_mode) {
			case ENGINE_ANALYSIS:
				if (to_play) {
					score_int =-score_int;
				}
				break;
			case ENGINE_BLACK:
				score_int =-score_int;
				break;
			case ENGINE_WHITE:
				if (to_play) {
					score_int =-score_int;
				}
				break;
		}
		char *evaluation;
		if (score_is_mate) {
			if (score_int == 0) {
				snprintf(scoreString, 16, to_play ? "1-0" : "0-1");
				set_analysis_best_line("");
			} else {
				evaluation = score_int > 0 ? "+ -" : "- +";
				snprintf(scoreString, 16, "%d", score_int);
				snprintf(scoreString, 16, "%s #(%d)", evaluation, score_int);
			}
		} else {
			if (abs(score_int) < 20) {
				evaluation = "=";
			} else if (score_int > 0 && score_int < 60) {
				evaluation = "⩲";
			} else if (score_int < 0 && score_int > -60) {
				evaluation = "⩱";
			} else if (score_int > 0 && score_int < 120) {
				evaluation = "±";
			} else if (score_int < 0 && score_int > -120) {
				evaluation = "∓";
			} else {
				evaluation = score_int > 0 ? "+ -" : "- +";
			}
			snprintf(scoreString, 16, "%s (%.2f)", evaluation, score_int / 100.0);
		}
		set_analysis_score(scoreString);
	}

	status = regexec(&info_best_line_matcher, info, 2, pmatch, 0);
	if (!status) {
		char best_line[BUFSIZ];
		char best_line_san[BUFSIZ];
		memset(best_line, 0, BUFSIZ);
		memset(best_line_san, 0, BUFSIZ);

		extract_match(info, pmatch[1], best_line);
		best_line_to_san(best_line, best_line_san);
//		debug("Best line in SAN: '%s'\n", best_line_san);

		size_t best_line_len = strlen(best_line);
		if (best_line_len > shown_best_line_len || strncmp(best_line, shown_best_line, best_line_len) != 0) {
//			debug("New best line: '%s' '%s'\n", best_line, shown_best_line);
			memcpy(shown_best_line, best_line, BUFSIZ);
			shown_best_line_len = best_line_len;
			set_analysis_best_line(best_line_san);
//		} else {
//			debug("best line NOT new: '%s' '%s'\n", best_line, shown_best_line);
		}
	}

	char depthString[32];
	char depth[8];
	memset(depth, 0, 8);
	status = regexec(&info_depth_matcher, info, 2, pmatch, 0);
	if (!status) {
		extract_match(info, pmatch[1], depth);
		snprintf(depthString, 32, "Depth: %s", depth);
		set_analysis_depth(depthString);
	}
	status = regexec(&info_selective_depth_matcher, info, 2, pmatch, 0);
	if (!status) {
		char sel_depth[8];
		extract_match(info, pmatch[1], sel_depth);
		snprintf(depthString, 32, "Depth: %s/%s", depth, sel_depth);
		set_analysis_depth(depthString);
	}

	char npsString[32];
	status = regexec(&info_nps_matcher, info, 2, pmatch, 0);
	if (!status) {
		char nps[16];
		extract_match(info, pmatch[1], nps);
		int npsInt = (int) strtol(nps, NULL, 10);
		snprintf(npsString, 32, "%d kN/s", npsInt / 1000);
		set_analysis_nodes_per_second(npsString);
	}

}

void best_line_to_san(char *line, char *san) {

	chess_game *trans_game = game_new();
	clone_game(main_game, trans_game);

	if (trans_game->whose_turn) {
		char move_num[16];
		sprintf(move_num, "%d...", trans_game->current_move_number);
		strcat(san, move_num);
	}

	char *left_over = line;
	char *parsed_to = line;
	size_t line_len = strlen(line);
	char move[6];
	while (left_over != NULL && left_over - line <= line_len) {
		left_over = strchr(left_over, ' ');
		if (left_over == NULL) {
			left_over = parsed_to + strlen(parsed_to);
		}
		memset(move, 0, 6);
		memcpy(move, parsed_to, left_over - parsed_to);

//		debug("best_line_to_san move is %s\n", move);
		int source_col = move[0] - 'a';
		int source_row = move[1] - '1';
		char promo_char = move[4];
		if (promo_char != '\0') {
			trans_game->promo_type = char_to_type(trans_game->whose_turn, (char) (promo_char - 32));
			debug("best_line_to_san  Move is a promotion! %s %c -> %c, %d\n", move, promo_char, (char) (promo_char - 32), trans_game->promo_type);
		}

		chess_piece *piece = trans_game->squares[source_col][source_row].piece;
		if (piece == NULL) {
			debug("best_line_to_san Ooops no piece here, was game restarted?! %c%d\n", source_col + 'a', source_row + 1);
			return;
		}

		if (!trans_game->whose_turn) {
			char move_num[16];
			sprintf(move_num, "%d. ", trans_game->current_move_number);
			strcat(san, move_num);
		}

		int resolved_move[4];
		int resolved = resolve_move(trans_game, piece->type, move, resolved_move);
		if (resolved) {
			char san_move[SAN_MOVE_SIZE];
			move_piece(trans_game->squares[resolved_move[0]][resolved_move[1]].piece, resolved_move[2], resolved_move[3], 0,
			           AUTO_SOURCE_NO_ANIM, san_move, trans_game, true);
			if (is_king_checked(trans_game, trans_game->whose_turn)) {
				if (is_check_mate(trans_game)) {
					san_move[strlen(san_move)] = '#';
				} else {
					san_move[strlen(san_move)] = '+';
				}
			}

			// Append move
			char pchar = san_move[0];
			int tt = char_to_type(trans_game->whose_turn, pchar);
			if (use_fig && tt != -1) {
				char buf_str[SAN_MOVE_SIZE];
				memset(buf_str, 0, SAN_MOVE_SIZE);
				// char_to_type() will return opposite colour because we swapped whose_turn already
				tt = colorise_type(tt, !trans_game->whose_turn);
				sprintf(buf_str, "%lc%s", type_to_unicode_char(tt), san_move + 1);
				strcpy(san_move, buf_str);
			}

			san_move[strlen(san_move)] = ' ';

			size_t movesLength = strlen(san);
			size_t newMoveLen = strlen(san_move);
			memcpy(san + movesLength, san_move, newMoveLen + 1); // includes terminating null
		} else {
			debug("best_line_to_san Could not resolve move %c%s\n", type_to_char(piece->type), move);
		}

		left_over++;
		parsed_to = left_over;

	}
	game_free(trans_game);

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
//	debug("Read from UCI '%s'\n", raw_buff);
	int i = 0;
	while (i > -1) {
		i = uci_scanner_lex();
		switch (i) {
			case UCI_OK: {
				set_uci_ok(true);
				break;
			}
			case UCI_READY: {
				set_uci_ready(true);
				break;
			}
			case UCI_ID_NAME: {
				debug("Got UCI Name: %s\n", uci_scanner_text);
				strcpy(engine_name, uci_scanner_text + 8);
				set_analysis_engine_name(engine_name);
				break;
			}
			case UCI_ID_AUTHOR: {
				debug("Got UCI Author: %s\n", uci_scanner_text);
				break;
			}
			case UCI_OPTION: {
				parse_option(uci_scanner_text);
				break;
			}
			case UCI_BEST_MOVE_NONE:
				handle_move_after_stop();
				break;
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

void *parse_uci_function(void *ignored) {
	fprintf(stdout, "[parse UCI thread] - Starting UCI parser\n");

	while (is_running_flag()) {
		parse_uci_buffer();
	}

	fprintf(stdout, "[parse UCI thread] - Closing UCI parser\n");
	return 0;
}

void write_to_uci(char *message) {
	pthread_mutex_lock(&uci_writer_lock);
	if (write(uci_in, message, strlen(message)) == -1) {
		perror("Failed to write to UCI engine ");
	}
	pthread_mutex_unlock(&uci_writer_lock);
	debug("Wrote to UCI: '%s'", message);
}

static void wait_for_engine_ready(void) {
	struct timeval start, now, diff;

	set_uci_ready(false);
	write_to_uci("isready\n");

	gettimeofday(&start, NULL);
	while (!is_uci_ready()) {
		usleep(10000);
		gettimeofday(&now, NULL);
		timersub(&now, &start, &diff);
		if (diff.tv_sec > READY_TIMEOUT_SEC) {
			debug("Ooops, UCI Engine did not reply to 'isready' within %d seconds, process crashed?!\n", READY_TIMEOUT_SEC);
			break;
		}
	}
}

static void stop_and_wait(void) {
	struct timeval start, now, diff;

	set_stop_requested(true);
	write_to_uci("stop\n");

	gettimeofday(&start, NULL);
	while (is_stop_requested()) {
		usleep(10000);
		gettimeofday(&now, NULL);
		timersub(&now, &start, &diff);
		if (diff.tv_sec > STOP_TIMEOUT_SEC) {
			printf("Ooops, UCI Engine did not stop in %d seconds will attempt to carry on anyway...\n", STOP_TIMEOUT_SEC);
			set_analysing(false);
			set_stop_requested(false);
			break;
		}
	}
}

static void real_start_uci_analysis() {
	debug("Starting UCI analysis from UCI manager\n");

	char moves[8192];
	if (ply_num == 1) {
		sprintf(moves, "%s\n", "position startpos");
	} else {
		pthread_mutex_lock(&all_moves_lock);
		sprintf(moves, "%s\n", all_moves);
		pthread_mutex_unlock(&all_moves_lock);
	}
	if (is_analysing()) {
		stop_and_wait();
	}
	wait_for_engine_ready();
	write_to_uci(moves);

	char go[256];
	if (uci_mode == ENGINE_ANALYSIS) {
		sprintf(go, "go infinite\n");
		set_analysing(true);
	} else {
		sprintf(go, "go wtime %ld btime %ld\n", get_remaining_time(main_clock, 0),
		        get_remaining_time(main_clock, 1));
	}
	write_to_uci(go);
}

static void real_start_uci_game() {
	debug("Starting new game from UCI manager\n");

	if (is_analysing()) {
		stop_and_wait();
	}
	wait_for_engine_ready();

	set_analysing(false);

	write_to_uci("ucinewgame\n");
	wait_for_engine_ready();

	char go[256];
	int relation;
	switch (uci_mode) {
		case ENGINE_WHITE:
			play_vs_machine = true;
			relation = -1;
			start_game("You", engine_name, game_time, 0, relation, true);
			// If engine is white, kick it now
			sprintf(go, "position startpos\ngo wtime %ld btime %ld\n", get_remaining_time(main_clock, 0), get_remaining_time(main_clock, 1));
			write_to_uci(go);
			break;
		case ENGINE_BLACK:
			play_vs_machine = true;
			relation = 1;
			start_game("You", engine_name, game_time, 0, relation, true);
			break;
		case ENGINE_ANALYSIS:
			play_vs_machine = false;
			break;
		default:
			break;
	}
}

void process_uci_actions() {
	char raw_buff[BUFSIZ];

	memset(raw_buff, 0, BUFSIZ);
	int nread = (int) read(uci_user_in[0], &raw_buff, BUFSIZ);
	if (nread < 1) {
		perror("Failed to read data from UCI user pipe ");
		return;
	}

	int j;
	char *initial_command;
	char *tokenize_save;

	char *all_tokens[200] = {NULL};
	unsigned int token_count = 0;
	for (j = 1, initial_command = raw_buff; ; j++, initial_command = NULL) {
		char *token = strtok_r(initial_command, "\n", &tokenize_save);
		if (token == NULL) {
			break;
		}
		all_tokens[token_count++] = token;
	}

	// Combine multiple start analysis into a single one
	token_count = 0;
	bool analysis_requested = false;
	while (all_tokens[token_count] != NULL) {
		char *token = all_tokens[token_count];
		if (!analysis_requested && !strcmp(START_ANALYSIS_TOKEN, token)) {
			analysis_requested = true;
		} else {
			if (analysis_requested) {
				real_start_uci_analysis();
				analysis_requested = false;
			}
			if (!strcmp(START_NEW_GAME_TOKEN, token)) {
				real_start_uci_game();
			}
		}
		token_count++;
	}
	if (analysis_requested) {
		real_start_uci_analysis();
	}
}

void *uci_manager_function(void *ignored) {
	fprintf(stdout, "[UCI manager thread] - Starting UCI manager\n");

	while (is_running_flag()) {
		process_uci_actions();
	}

	fprintf(stdout, "[UCI manager thread] - UCI manager terminated\n");
	return 0;
}

