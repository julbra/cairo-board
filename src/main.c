#include <stdlib.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <sys/time.h>
#include <execinfo.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <pthread.h>
#include <getopt.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include "freetype/freetype.h" FT_FREETYPE_H

#include "cairo-board.h"
#include "configuration.h"
#include "netstuff.h"
#include "chess-backend.h"
#include "drawing-backend.h"
#include "san_scanner.h"
#include "ics_scanner.h"
#include "clock-widget.h"
#include "crafty-adapter.h"
#include "uci-adapter.h"
#include "channels.h"
#include "analysis_panel.h"
#include "test.h"
#include "ics-adapter.h"

/* check that C's multibyte output is supported for use with figurine characters */
#ifndef __STDC_ISO_10646__
#error "This compiler/libc doesn't support C's multibyte output!"
#endif

void print_trace(void) {
	void *array[50];
	size_t size, i;
	char **strings;

	size = backtrace(array, 50);
	strings = backtrace_symbols(array, size);

	fprintf(stderr, "Obtained %zd stack frames.\n", size);

	// start from one as the first frame is *this* function
	for (i = 1; i < size; i++) {
		fprintf(stderr, "%s\n", strings[i]);
	}
	free(strings);
}

void sig_handler(int sig) {
	switch(sig) {
		case SIGINT:
			gtk_main_quit();
			break;
		case SIGABRT:
		case SIGSEGV:
			signal(sig, SIG_IGN);
			print_trace();
			break;
		default:
			break;
	}
}

/* <Options variables> */
gboolean debug_flag = FALSE;
gboolean ics_mode = FALSE;
bool guest_mode = false;

gboolean use_fig = FALSE;
gboolean show_last_move = FALSE;
gboolean always_promote_to_queen = FALSE;
gboolean highlight_moves = FALSE;
bool highlight_last_move = true;

gboolean test_first_player = FALSE;

char ics_host[256];
char default_ics_host[] = "freechess.org";

bool clock_started = false;
bool game_started = false;
long my_game = 0;

// names may be at most 17 characters long
char ics_test_my_handle[32];
char ics_handle1[32];
char default_ics_handle1[] = "cairoboardone";
char ics_test_opp_handle[32];
char ics_handle2[32];
char default_ics_handle2[] = "cairoboardtwo";

unsigned short ics_port;
unsigned short default_ics_port = 5000;
char file_to_load[PATH_MAX];
unsigned int game_to_load = 1;
unsigned int auto_play_delay = 1000;

bool ics_host_specified = false;
bool ics_port_specified = false;
bool load_file_specified = false;
bool ics_handle1_specified = false;
bool ics_handle2_specified = false;
/* </Options variables> */

bool delay_from_promotion = false;
int p_old_col, p_old_row;
char last_san_move[SAN_MOVE_SIZE];
int resolved_move[4];


// clocks variables
chess_clock *main_clock;

// <premove variables>
int premove_old_col = -1;
int premove_old_row = -1;
int premove_new_col = -1;
int premove_new_row = -1;
int premove_promo_type = -1;
int got_premove = 0;
// </premove variables>

/* GUI variables */
double svg_w, svg_h;
static guint clock_board_ratio = 20;
GtkWidget *main_window;
GtkWidget *board;
GtkWidget *label_frame;
GtkWidget *label_frame_event_box;

// moves list text box
static GtkWidget *moves_list_view;
static GtkTextBuffer *moves_list_buffer;
static GtkWidget* scrolled_window;
GtkWidget* moves_list_title_label;
static GtkWidget* opening_code_label;
static GtkWidget* goto_first_button;
static GtkWidget* goto_last_button;
static GtkWidget* go_back_button;
static GtkWidget* go_forward_button;
//static GtkWidget* play_pause_button;


static GtkWidget *clock_widget;

static int last_move_x, last_move_y;
static int last_release_x, last_release_y;
static double dragging_prev_x = 0;
static double dragging_prev_y = 0;
static bool board_flipped = 0;
static int pre_move[4];

bool playing = false;
static guint de_scale_timer = 0;
static guint auto_play_timer = 0;
static guint clock_refresher = 0;

int old_wi, old_hi;
double w_ratio = 1.0;
double h_ratio = 1.0;

// FreeType
FT_Library library;

// globals
int mouse_clicked[2] = {-1, -1};
int type;
char currentMoveString[5]; // accommodate for one move

extern uint64_t zobrist_keys_squares[8][8][12];
extern uint64_t zobrist_keys_en_passant[8];
extern uint64_t zobrist_keys_blacks_turn;
extern uint64_t zobrist_keys_castle[2][2];

/* *** <Current game State machine variables> *** */
// Rule engine variables

chess_piece *last_piece_taken = NULL;

// Game Metadata
plys_list *main_list;

int game_mode = MANUAL_PLAY;

/* *** </Current game State machine variables> *** */

chess_piece *mouse_clicked_piece = NULL;
chess_piece *mouse_dragged_piece = NULL;
chess_piece *auto_selected_piece = NULL;
chess_piece *king_in_check_piece = NULL;

int needs_update;
int needs_scale;

double dr = 181.0/255.0;
double dg = 136.0/255.0;
double db = 99.0/255.0;
double lr = 240.0/255.0;
double lg = 217.0/255.0;
double lb = 181.0/255.0;
GdkRGBA chat_handle_colour;
bool invert_fig_colours;

double highlight_selected_r;
double highlight_selected_g;
double highlight_selected_b;
double highlight_selected_a = 0.5;
double highlight_move_r;
double highlight_move_g;
double highlight_move_b;
double highlight_move_a = 0.5;
double highlight_pre_move_r;
double highlight_pre_move_g;
double highlight_pre_move_b;
double highlight_pre_move_a = 0.5;
double check_warn_r = 1.0;
double check_warn_g = 0.1;
double check_warn_b = 0.1;
double check_warn_a = 1.0;

/* Prototypes */
char *get_eco_full(const char *san_moves_list);
char *get_eco_long(const char *fen_key);
char *get_eco_short(const char *fen_key);
wint_t type_to_unicode_char(int type);

int open_file(const char*);
gboolean auto_play_one_move(gpointer data);
gboolean auto_play_one_ics_move(gpointer data);
void reset_moves_list_view(gboolean lock_threads);
gboolean auto_play_one_crafty_move(gpointer data);
gboolean auto_play_one_uci_move(gpointer data);

static void reset_game(bool lock_threads);
static void parse_ics_buffer(void);

void send_to_ics(char*);

void set_header_label(const char *w_name, const char *b_name, const char *w_rating, const char *b_rating);

static void get_int_from_popup(void);
static void spawn_mover(void);

/************************ <MULTITHREAD STUFF> ******************************/
static bool moveit_flag;
static bool running_flag;
static bool more_events_flag;

pthread_mutex_t mutex_last_move = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t last_move_xy_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t last_release_xy_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dragging_prev_xy_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t moveit_flag_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t running_flag_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t more_events_flag_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t board_flipped_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pre_move_lock = PTHREAD_MUTEX_INITIALIZER;

void cleanup_mutexes(void) {
	pthread_mutex_destroy(&mutex_last_move);
	pthread_mutex_destroy(&last_move_xy_lock);
	pthread_mutex_destroy(&last_release_xy_lock);
	pthread_mutex_destroy(&dragging_prev_xy_lock);
	pthread_mutex_destroy(&moveit_flag_lock);
	pthread_mutex_destroy(&running_flag_lock);
	pthread_mutex_destroy(&more_events_flag_lock);
	pthread_mutex_destroy(&board_flipped_lock);
	pthread_mutex_destroy(&pre_move_lock);
}

char last_move[MOVE_BUFF_SIZE];

void set_last_move(char *move) {
	pthread_mutex_lock(&mutex_last_move);
	strncpy(last_move, move, MOVE_BUFF_SIZE);
	pthread_mutex_unlock(&mutex_last_move);
}

void get_last_move(char *data) {
	pthread_mutex_lock(&mutex_last_move);
	strncpy(data, last_move, MOVE_BUFF_SIZE);
	pthread_mutex_unlock(&mutex_last_move);
}

void get_last_move_xy(int *x, int *y) {
	pthread_mutex_lock(&last_move_xy_lock);
	*x = last_move_x;
	*y = last_move_y;
	pthread_mutex_unlock(&last_move_xy_lock);
}

void set_last_move_xy(int x, int y) {
	pthread_mutex_lock(&last_move_xy_lock);
	last_move_x = x;
	last_move_y = y;
	pthread_mutex_unlock(&last_move_xy_lock);
}

void get_last_release_xy(int *x, int *y) {
	pthread_mutex_lock(&last_release_xy_lock);
	*x = last_release_x;
	*y = last_release_y;
	pthread_mutex_unlock(&last_release_xy_lock);
}

void set_last_release_xy(int x, int y) {
	pthread_mutex_lock(&last_release_xy_lock);
	last_release_x = x;
	last_release_y = y;
	pthread_mutex_unlock(&last_release_xy_lock);
}

void get_dragging_prev_xy(double *x, double *y) {
	pthread_mutex_lock(&dragging_prev_xy_lock);
	*x = dragging_prev_x;
	*y = dragging_prev_y;
	pthread_mutex_unlock(&dragging_prev_xy_lock);
}

void set_dragging_prev_xy(double x, double y) {
	pthread_mutex_lock(&dragging_prev_xy_lock);
	dragging_prev_x = x;
	dragging_prev_y = y;
	pthread_mutex_unlock(&dragging_prev_xy_lock);
}

void set_moveit_flag(bool val) {
	pthread_mutex_lock(&moveit_flag_lock);
	moveit_flag = val;
	pthread_mutex_unlock(&moveit_flag_lock);
}

bool is_moveit_flag() {
	bool val;
	pthread_mutex_lock(&moveit_flag_lock);
	val = moveit_flag;
	pthread_mutex_unlock(&moveit_flag_lock);
	return val;
}

void set_running_flag(bool val) {
	pthread_mutex_lock(&running_flag_lock);
	running_flag = val;
	pthread_mutex_unlock(&running_flag_lock);
}

bool is_running_flag() {
	bool val;
	pthread_mutex_lock(&running_flag_lock);
	val = running_flag;
	pthread_mutex_unlock(&running_flag_lock);
	return val;
}

void set_more_events_flag(bool val) {
	pthread_mutex_lock(&more_events_flag_lock);
	more_events_flag = val;
	pthread_mutex_unlock(&more_events_flag_lock);
}

bool is_more_events_flag() {
	bool val;
	pthread_mutex_lock(&more_events_flag_lock);
	val = more_events_flag;
	pthread_mutex_unlock(&more_events_flag_lock);
	return val;
}

void set_board_flipped(bool val) {
	pthread_mutex_lock(&board_flipped_lock);
	board_flipped = val;
	pthread_mutex_unlock(&board_flipped_lock);
}

bool is_board_flipped() {
	bool val;
	pthread_mutex_lock(&board_flipped_lock);
	val = board_flipped;
	pthread_mutex_unlock(&board_flipped_lock);
	return val;
}

void get_pre_move(int premove[4]) {
	pthread_mutex_lock(&pre_move_lock);
	for (int i = 0; i < 4; ++i) {
		premove[i] = pre_move[i];
	}
	pthread_mutex_unlock(&pre_move_lock);
}

void set_pre_move(int premove[4]) {
	pthread_mutex_lock(&pre_move_lock);
	for (int i = 0; i < 4; ++i) {
		pre_move[i] = premove[i];
	}
	pthread_mutex_unlock(&pre_move_lock);
}

void unset_pre_move() {
	pthread_mutex_lock(&pre_move_lock);
	for (int i = 0; i < 4; ++i) {
		pre_move[i] = -1;
	}
	pthread_mutex_unlock(&pre_move_lock);
}

/************************ </MULTITHREAD STUFF> *****************************/


/******** <Signals Stuff> ***********/
void create_signals(void) {
	g_signal_new("got-move", GTK_TYPE_WIDGET, G_SIGNAL_RUN_FIRST, 0, NULL, NULL, /*marshaller*/g_cclosure_marshal_VOID__VOID, /*return type*/G_TYPE_NONE, 0);
	g_signal_new("got-crafty-move", GTK_TYPE_WIDGET, G_SIGNAL_RUN_FIRST, 0, NULL, NULL, /*marshaller*/g_cclosure_marshal_VOID__VOID, /*return type*/G_TYPE_NONE, 0);
	g_signal_new("got-uci-move", GTK_TYPE_WIDGET, G_SIGNAL_RUN_FIRST, 0, NULL, NULL, /*marshaller*/g_cclosure_marshal_VOID__VOID, /*return type*/G_TYPE_NONE, 0);
	g_signal_new("flip-board", GTK_TYPE_WIDGET, G_SIGNAL_RUN_FIRST, 0, NULL, NULL, /*marshaller*/g_cclosure_marshal_VOID__VOID, /*return type*/G_TYPE_NONE, 0);
}
/******** </Signals Stuff> ***********/


/******** <XY to IJ mapping helpers> ***********/
void xy_to_loc(int x, int y, int *pos, int wi, int hi) {
	bool flipped = is_board_flipped();
	int a = (int) 8*x/wi;
	int b = (int) 8*y/hi;
	pos[0] = (flipped ? 7 - a : a) ;
	pos[1] = (flipped ? b : 7 - b);
}

/* Returns the square at x,y coordinates */
chess_square *xy_to_square(chess_game *game, int x, int y, int wi, int hi) {
	int ij[2];
	xy_to_loc(x, y, ij, wi, hi);
	return &game->squares[ij[0]][ij[1]];
}

/* Converts column,row to x,y coordinates */
void ij_to_xy(int i, int j, double *xy, int wi, int hi) {
	bool flipped = is_board_flipped();
	xy[0] = (wi * ((flipped ? 7 - i : i) + .5f)) / 8;
	xy[1] = (hi * ((flipped ? 7 - j : j) + .5f)) / 8;
}

void loc_to_xy(int column, int row, double *xy, int wi, int hi) {
	ij_to_xy(column, 7-row, xy, wi, hi);
}

void piece_to_xy(chess_piece *piece, double *xy ,int wi, int hi) {
	loc_to_xy(piece->pos.column, piece->pos.row, xy, wi, hi);
}
/******** </XY to IJ mapping helpers> ***********/

extern cairo_surface_t *piece_surfaces[12];

void assign_surfaces() {
	int i;
	for (i = 0; i < 16; i++) {
		main_game->white_set[i].surf = piece_surfaces[main_game->white_set[i].type];
		main_game->black_set[i].surf = piece_surfaces[main_game->black_set[i].type];
	}
}

static gboolean on_board_draw(GtkWidget *pWidget, cairo_t *cdr) {

	double wi = (double) (gtk_widget_get_allocated_width(pWidget));
	double hi = (double) (gtk_widget_get_allocated_height(pWidget));

	if (needs_update) {
		draw_full_update(cdr, wi, hi);
	} else if (needs_scale) {
		draw_scaled(cdr, wi, hi);
	} else {
		draw_cheap_repaint(cdr, wi, hi);
	}

	return TRUE;
}

void flip_board(int wi, int hi) {
	pthread_mutex_lock(&board_flipped_lock);
	board_flipped = !board_flipped;
	pthread_mutex_unlock(&board_flipped_lock);
	draw_board_surface(wi, hi);
	cairo_t *cdr = gdk_cairo_create(gtk_widget_get_window(board));
	draw_full_update(cdr, wi, hi);
	cairo_destroy(cdr);
}

int char_to_type(int whose_turn, char c) {
	switch(c) {
		case 'R':
			return (whose_turn ? B_ROOK : W_ROOK);
		case 'B':
			return (whose_turn ? B_BISHOP : W_BISHOP);
		case 'N':
			return (whose_turn ? B_KNIGHT : W_KNIGHT);
		case 'Q':
			return (whose_turn ? B_QUEEN : W_QUEEN);
		case 'K':
			return (whose_turn ? B_KING : W_KING);
		case 'P':
			return (whose_turn ? B_PAWN : W_PAWN);
		default:
			break;
	}
	return -1;
}

char type_to_char(int type) {
	switch (type) {
		case W_ROOK:
		case B_ROOK:
			return 'R';
		case W_BISHOP:
		case B_BISHOP:
			return 'B';
		case W_KNIGHT:
		case B_KNIGHT:
			return 'N';
		case W_QUEEN:
		case B_QUEEN:
			return 'Q';
		case W_KING:
		case B_KING:
			return 'K';
		case W_PAWN:
		case B_PAWN:
			return (char) 0;
		default:
			return (char) 0;
	}
}

char type_to_fen_char(int type) {
	switch (type) {
		case W_ROOK:
			return 'R';
		case B_ROOK:
			return 'r';
		case W_BISHOP:
			return 'B';
		case B_BISHOP:
			return 'b';
		case W_KNIGHT:
			return 'N';
		case B_KNIGHT:
			return 'n';
		case W_QUEEN:
			return 'Q';
		case B_QUEEN:
			return 'q';
		case W_KING:
			return 'K';
		case B_KING:
			return 'k';
		case W_PAWN:
			return 'P';
		case B_PAWN:
			return 'p';
		default:
			return (char) 0;
	}
}

// move is legal so we can make assumptions
int is_move_castle(chess_piece *piece, int col, int row) {
	if (piece->type != W_KING && piece->type != B_KING) {
		return 0;
	}
	if (piece->type == W_KING) {
		if (col == piece->pos.column - 2) {
			return CASTLE | W_CASTLE_LEFT;
		}
		if (col == piece->pos.column + 2) {
			return CASTLE | W_CASTLE_RIGHT;
		}
	}
	else {
		if (col == piece->pos.column - 2) {
			return CASTLE | B_CASTLE_LEFT;
		}
		if (col == piece->pos.column + 2) {
			return CASTLE | B_CASTLE_RIGHT;
		}
	}
	return 0;
}

// move is legal so we can make assumptions
int is_move_double_pawn_push(chess_piece *piece, int col, int row) {
	if (piece->type != W_PAWN && piece->type != B_PAWN) {
		return 0;
	}

	if (piece->type == W_PAWN) {
		if (piece->pos.row != 1) {
			return 0;
		}
		if (row == 3) {
			return 1;
		}
	}
	else {
		if (piece->pos.row != 6) {
			return 0;
		}
		if (row == 4) {
			return 1;
		}
	}
	return 0;
}

// move is legal so we can make assumptions
int is_move_promotion(chess_piece *piece, int col, int row) {
	if (piece->type != W_PAWN && piece->type != B_PAWN) {
		return 0;
	}

	if (!piece->colour) {
		if (row == 7) {
			return PROMOTE;
		}
	}
	else {
		if (row == 0) {
			return PROMOTE;
		}
	}
	return 0;
}

// move is legal so we can make assumptions
int is_move_capture(chess_game *game, chess_piece *piece, int col, int row) {
	if (game->squares[col][row].piece != NULL) {
		last_piece_taken = game->squares[col][row].piece;
		return PIECE_TAKEN;
	}
	return 0;
}

chess_piece *to_promote;
gboolean has_chosen;

static void get_int_from_popup(void) {

	has_chosen = FALSE;

	GtkWidget *promote_popup = gtk_menu_new();

	GtkWidget *queen_item;
	GtkWidget *rook_item;
	GtkWidget *bishop_item;
	GtkWidget *knight_item;

	char item_text[32];
	sprintf(item_text, "%lc: _Queen", type_to_unicode_char(main_game->whose_turn ? B_QUEEN : W_QUEEN));
	queen_item = gtk_menu_item_new_with_mnemonic(item_text);
	sprintf(item_text, "%lc: _Rook", type_to_unicode_char(main_game->whose_turn ? B_ROOK : W_ROOK));
	rook_item = gtk_menu_item_new_with_mnemonic(item_text);
	sprintf(item_text, "%lc: _Bishop", type_to_unicode_char(main_game->whose_turn ? B_BISHOP : W_BISHOP));
	bishop_item = gtk_menu_item_new_with_mnemonic(item_text);
	sprintf(item_text, "%lc: _Knight", type_to_unicode_char(main_game->whose_turn ? B_KNIGHT : W_KNIGHT));
	knight_item = gtk_menu_item_new_with_mnemonic(item_text);

	g_signal_connect(queen_item, "activate", G_CALLBACK (choose_promote_handler), GINT_TO_POINTER(W_QUEEN));
	g_signal_connect(rook_item, "activate", G_CALLBACK (choose_promote_handler), GINT_TO_POINTER(W_ROOK));
	g_signal_connect(bishop_item, "activate", G_CALLBACK (choose_promote_handler), GINT_TO_POINTER(W_BISHOP));
	g_signal_connect(knight_item, "activate", G_CALLBACK (choose_promote_handler), GINT_TO_POINTER(W_KNIGHT));

	g_signal_connect(promote_popup, "deactivate", G_CALLBACK (choose_promote_deactivate_handler), GINT_TO_POINTER(-1));

//	gtk_widget_set_size_request (promote_popup, 100, 6+30*4);

//	gtk_widget_set_size_request (queen_item, 100, 30);
//	gtk_widget_set_size_request (rook_item, 100, 30);
//	gtk_widget_set_size_request (bishop_item, 100, 30);
//	gtk_widget_set_size_request (knight_item, 100, 30);

	gtk_widget_show (queen_item);
	gtk_widget_show (rook_item);
	gtk_widget_show (bishop_item);
	gtk_widget_show (knight_item);

	gtk_menu_attach( GTK_MENU(promote_popup), queen_item, 0, 1, 0, 1);
	gtk_menu_attach( GTK_MENU(promote_popup), rook_item, 0, 1, 1, 2);
	gtk_menu_attach( GTK_MENU(promote_popup), bishop_item, 0, 1, 2, 3);
	gtk_menu_attach( GTK_MENU(promote_popup), knight_item, 0, 1, 3, 4);

	/* popup the choice menu */
	gtk_menu_popup ( GTK_MENU(promote_popup), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}


void init_castle_state(chess_game *game) {
	int i, j;
	for (i=0; i<2; i++)
		for (j=0; j<2; j++)
			game->castle_state[i][j] = 1;
}

/* basic sanity check to prevent moving while observing or moving oponent's pieces */
bool can_i_move_piece(chess_piece *piece) {
	switch (game_mode) {
		case I_PLAY_WHITE:
			return !piece->colour;
		case I_PLAY_BLACK:
			return piece->colour;
		case I_OBSERVE:
			return false;
		case MANUAL_PLAY:
			return true;
		default:
			return false;
	}
}

int move_piece(chess_piece *piece, int col, int row, int check_legality, int move_source, char san_move[SAN_MOVE_SIZE], chess_game *game, bool only_logical) {

	// Determine whether proposed move is legal
	if (!check_legality || is_move_legal(game, piece, col, row)) {

		int reset_fifty_counter = 0;
		int was_castle = is_move_castle(piece, col, row);
		int was_double_pawn_push = is_move_double_pawn_push(piece, col, row);
		int was_en_passant = is_move_en_passant(game, piece, col, row);
		int was_promotion = is_move_promotion(piece, col, row);
		int piece_taken = (was_en_passant || is_move_capture(game, piece, col, row)) ? PIECE_TAKEN : 0;

		char move_in_san[SAN_MOVE_SIZE];
		// Build the san move
		memset(move_in_san, 0, SAN_MOVE_SIZE);

		/* handle castle special case */
		if (was_castle) {
			switch (was_castle & MOVE_DETAIL_MASK) {
				case W_CASTLE_LEFT:
				case B_CASTLE_LEFT:
					strncpy(move_in_san, "O-O-O", 6);
					break;
				case W_CASTLE_RIGHT:
				case B_CASTLE_RIGHT:
					strncpy(move_in_san, "O-O", 4);
					break;
				default:
					// Bug if it happens, print error and exit
					fprintf(stderr, "ERROR: error in castling code!\n");
					exit(1);
					break;
			}
		} else {

			int offset = 0;

			/* piece type letter */
			char ptype = type_to_char(piece->type);
			if (ptype) {
				move_in_san[offset++] = ptype;
			}

			/* do we need a disambiguator? */
			int disambiguator_need = 0; // 1 column, 2 row, 3 both
			if (!ptype) {
				if (piece_taken) { // special pawn-taking case
					disambiguator_need = 1;
				}
			} else { // non-pawn case
				/* remember this turn's whose_turn swap hasn't happened yet */
				chess_piece *set = game->whose_turn ? game->black_set : game->white_set;

				int i, j;
				for (i = 0; i < 16; i++) {
					chess_piece competitor = set[i];
					if (!competitor.dead && competitor.type == piece->type &&
					    (competitor.pos.row != piece->pos.row || competitor.pos.column != piece->pos.column)) {
						if (piece->type == B_KING || piece->type == W_KING) {
							for (i = 0; i < 16; i++) {
								debug("Piece[%d]: %c %c%c\n", i, type_to_char(set[i].type), set[i].pos.column + 'a',
								      set[i].pos.row + '1');
							}
							fprintf(stderr, "Disambiguating for king?! %d %p %p\n", (&competitor != piece), &competitor,
							        piece);
							debug("The Piece: %c %c%d\n", type_to_char(piece->type), piece->pos.column + 'a',
							      piece->pos.row + 1);
						}
						/* found a piece from same set with same type
						 * we now check whether it can go to the same dest */
						int possible_moves[64][2];
						int count = get_possible_moves(game, &competitor, possible_moves, 0);
						for (j = 0; j < count; j++) {
							if (possible_moves[j][0] == col && possible_moves[j][1] == row) {
								if (competitor.pos.column != piece->pos.column) {
									disambiguator_need |= 1;
								} else {
									disambiguator_need |= 2;
								}
								break;
							}
						}
					}
				}
			}

			switch (disambiguator_need) {
				case 1:
					move_in_san[offset++] = (char) ('a' + piece->pos.column);
					break;
				case 2:
					move_in_san[offset++] = (char) ('1' + piece->pos.row);
					break;
				case 3:
					move_in_san[offset++] = (char) ('a' + piece->pos.column);
					move_in_san[offset++] = (char) ('1' + piece->pos.row);
					break;
				default:
					break;
			}

			// was piece taken?
			if (piece_taken) {
				move_in_san[offset++] = 'x';
			}

			// dest location
			move_in_san[offset++] = (char) (col + 'a');
			move_in_san[offset] = (char) (row + '1');

			// promotions handled later in SAN string
		}

		int ocol, orow;
		ocol = piece->pos.column;
		orow = piece->pos.row;

		/* Raw move */
		raw_move(game, piece, col, row, 1);

		// Reset the 50 move counter if piece was a pawn or a piece was taken
		if (piece->type == W_PAWN || piece->type == B_PAWN) {
			reset_fifty_counter = 1;
		} else if (piece_taken) {
			reset_fifty_counter = 1;
		}

		// handle special pawn en-passant move
		// NOTE: no need to reset 50 counter as done already
		if (was_en_passant) {
			// get square where pawn to kill is
			chess_square *to_kill = &(game->squares[col][row + (game->whose_turn ? 1 : -1)]);

			// remove piece from hash
			toggle_piece(game, to_kill->piece);

			// kill pawn
			to_kill->piece->dead = true;
			to_kill->piece = NULL;
		}

		// handle special promotion move
		// NOTE: no need to reset 50 counter as done already
		if (was_promotion) {
			to_promote = piece;
			if (move_source == MANUAL_SOURCE) {
				if (!always_promote_to_queen) {
					get_int_from_popup();
					delay_from_promotion = true;
				} else {
					delay_from_promotion = false;
					strcat(move_in_san, "=Q");
					choose_promote(1, false, only_logical, ocol, orow, col, row);
				}
			} else {
				delay_from_promotion = false; // this means we can print the move when we return from this
				char promo_string[8];
				memset(promo_string, 0, 8);
				if (use_fig) {
					sprintf(promo_string, "=%lc", type_to_unicode_char(colorise_type(game->promo_type, game->whose_turn)));
				}
				else {
					sprintf(promo_string, "=%c", type_to_char(game->promo_type));
				}
				strcat(move_in_san, promo_string);

				if (move_source == AUTO_SOURCE_NO_ANIM) {
					choose_promote(game->promo_type, false, only_logical, ocol, orow, col, row);
					// If animating, handle promotion at end of the animation (because it's prettier!)
				}
			}
		} else {
			delay_from_promotion = false;
		}

		if (!delay_from_promotion) {

		}

		if (san_move != NULL) {
			memcpy(san_move, move_in_san, SAN_MOVE_SIZE);
		}

		if (was_castle) {
			int old_col, old_row, new_col;
			switch (was_castle & MOVE_DETAIL_MASK) {
				case W_CASTLE_LEFT:
					old_col = 0;
					old_row = 0;
					new_col = 3;
					break;
				case W_CASTLE_RIGHT:
					old_col = 7;
					old_row = 0;
					new_col = 5;
					break;
				case B_CASTLE_LEFT:
					old_col = 0;
					old_row = 7;
					new_col = 3;
					break;
				case B_CASTLE_RIGHT:
					old_col = 7;
					old_row = 7;
					new_col = 5;
					break;
				default:
					// Bug if it happens
					fprintf(stderr, "ERROR: bug in castling code!\n");
					exit(1);
					break;
			}
			chess_piece *rook = game->squares[old_col][old_row].piece;
			// Actual move
			raw_move(game, rook, new_col, rook->pos.row, 1);
		}

		// disable castling state switches if needed and some are still set
		int i,j;
		bool needs_check = false;
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				if (game->castle_state[i][j]) {
					needs_check = true;
					break;
				}
			}
			if (needs_check) break;
		}
		if (needs_check) {
			switch (piece->type) {
				case W_ROOK:
					if (game->castle_state[0][0] && !piece->pos.column && !piece->pos.row) {
						if (game->castle_state[0][0]) {
							game->castle_state[0][0] = 0;
							game->current_hash ^= zobrist_keys_castle[0][0];
						}
					} else if (game->castle_state[0][1] && piece->pos.column == 7 && !piece->pos.row) {
						if (game->castle_state[0][1]) {
							game->castle_state[0][1] = 0;
							game->current_hash ^= zobrist_keys_castle[0][1];
						}
					}
					break;
				case B_ROOK:
					if (game->castle_state[1][0] && !piece->pos.column && piece->pos.row == 7) {
						if (game->castle_state[1][0]) {
							game->castle_state[1][0] = 0;
							game->current_hash ^= zobrist_keys_castle[1][0];
						}
					} else if (game->castle_state[1][1] && piece->pos.column == 7 && piece->pos.row == 7) {
						if (game->castle_state[1][1]) {
							game->castle_state[1][1] = 0;
							game->current_hash ^= zobrist_keys_castle[1][1];
						}
					}
					break;
				case W_KING:
					if (game->castle_state[0][0]) {
						game->castle_state[0][0] = 0;
						game->current_hash ^= zobrist_keys_castle[0][0];
					}
					if (game->castle_state[0][1]) {
						game->castle_state[0][1] = 0;
						game->current_hash ^= zobrist_keys_castle[0][1];
					}
					break;
				case B_KING:
					if (game->castle_state[1][0]) {
						game->castle_state[1][0] = 0;
						game->current_hash ^= zobrist_keys_castle[1][0];
					}
					if (game->castle_state[1][1]) {
						game->castle_state[1][1] = 0;
						game->current_hash ^= zobrist_keys_castle[1][1];
					}
					break;
				default:
					// do nothing
					break;
			}
		}

		// Handle en-passant swicthes

		// Disable old switches since they are obsolete
		reset_en_passant(game);

		// Enable potential new switches
		if (was_double_pawn_push) {
			game->en_passant[col] = 1;
			game->current_hash ^= zobrist_keys_en_passant[col];
		}

		// Reset fifty move counter
		if (reset_fifty_counter) {
			game->fifty_move_counter = 100;
		}

		// Increase full move number
		if (game->whose_turn) {
			game->current_move_number++;
		}

		// Swap turns
		game->whose_turn = !game->whose_turn;

		// Decrement fifty move counter
		game->fifty_move_counter--;

		game->current_hash ^= zobrist_keys_blacks_turn;

		persist_hash(game);

		return was_castle | piece_taken | was_en_passant | was_promotion;
	}
	else {
		return -1;
	}
}

void update_eco_tag(bool should_lock_threads) {
	char *eco_full = get_eco_full(main_game->moves_list);
	if (eco_full) {
		char eco[128];
		char eco_description[128];
		memset(eco_description, 0, 128);
		memset(eco, 0, 128);
		char eco_code[4];
		memcpy(eco_code, eco_full, 3);
		eco_code[3] = '\0';
		strncpy(eco_description, eco_full + 4, 128);
		snprintf(eco, 128, "<span weight=\"bold\">%s</span> %s", eco_code, eco_description);
		if (should_lock_threads) {
			gdk_threads_enter();
		}
		gtk_label_set_markup(GTK_LABEL(opening_code_label), eco);
		gtk_widget_set_tooltip_text(opening_code_label, eco_description);
		if (should_lock_threads) {
			gdk_threads_leave();
		}
	}
}

void check_ending_clause(chess_game *game) {
	if (is_king_checked(game, game->whose_turn)) {
		if (is_check_mate(game)) {
			last_san_move[strlen(last_san_move)] = '#';
			printf("Checkmate! %s wins\n", (game->whose_turn ? "White" : "Black"));
		} else {
			last_san_move[strlen(last_san_move)] = '+';
		}
	} else if (is_stale_mate(game)) {
		printf("Stalemate! Game drawn\n");
	} else if (check_hash_triplet(game)) {
		printf("Game drawn by repetition\n");
		send_to_ics("draw\n");
	} else if (is_material_draw(game->white_set, game->black_set)) {
		printf("Insufficient material! Game drawn\n");
		send_to_ics("draw\n");
	} else if (is_fifty_move_counter_expired(game)) {
		printf("Game drawn by 50 move rule\n");
		send_to_ics("draw\n");
	}
}

static gboolean on_button_press(GtkWidget *pWidget, GdkEventButton *pButton, GdkWindowEdge edge) {

	if (pButton->type == GDK_BUTTON_PRESS) {

		int wi = gtk_widget_get_allocated_width(pWidget);
		int hi = gtk_widget_get_allocated_height(pWidget);

		if (pButton->button == 1) {
			handle_left_button_press(pWidget, wi, hi, pButton->x, pButton->y);
		}
		else if (pButton->button == 3) {
			handle_right_button_press(pWidget, wi, hi);
		}
		else if (pButton->button == 2) {
			reset_game(false);
			reset_board();
			reset_moves_list_view(FALSE);
			handle_middle_button_press(pWidget, wi, hi);
		}
	}
	return TRUE;
}

static gboolean on_button_release (GtkWidget *pWidget, GdkEventButton *pButton, GdkWindowEdge edge) {
	if (pButton->type == GDK_BUTTON_RELEASE) {
		if (pButton->button == 1) {
			set_last_release_xy((int) pButton->x, (int) pButton->y);
			handle_button_release();
		}
	}
	return TRUE;
}

static gboolean on_motion(GtkWidget *pWidget, GdkEventMotion *event) {
	// Grab the last event
	if (is_moveit_flag()) {
		set_last_move_xy((int) event->x, (int) event->y);
		set_more_events_flag(true);
	}
	return TRUE;
}

gboolean de_scale(gpointer data) {
//	printf("De-scale\n");
	needs_update = 1;
	gtk_widget_queue_draw(GTK_WIDGET(data));
	de_scale_timer = 0;
	// Only fire once
	return FALSE;
}

static gboolean on_get_move(GtkWidget *pWidget) {
	debug("Got Move\n");
	auto_play_one_ics_move(pWidget);

	// Kludge to wake-up the window
	g_main_context_wakeup(NULL);

	return true;
}

static gboolean on_get_crafty_move(GtkWidget *pWidget) {
	debug("Got Crafty Move\n");
	auto_play_one_crafty_move(pWidget);

	// Kludge to wake-up the window
	g_main_context_wakeup(NULL);

	return true;
}

static gboolean on_get_uci_move(GtkWidget *pWidget) {
	debug("Got UCI Move\n");
	auto_play_one_uci_move(pWidget);
	// Kludge to wake-up the window
	g_main_context_wakeup(NULL);
	return true;
}

static gboolean on_flip_board(GtkWidget *pWidget) {
	handle_flip_board(pWidget, true);
	return TRUE;
}

static int first_configure = 1;
static int last_alloc_wi = 0;
static int last_alloc_hi = 0;

static gboolean on_configure_event(GtkWidget *pWidget, GdkEventConfigure *event) {

	if (pWidget == board) {
		// This is a board resize event

//		debug("Got resize: %d\n", gtk_widget_get_allocated_width(board));
		// Check whether the board dimensions actually changed
		// NB: because we enforce a 1:1 ratio we only need to check the width but
		// for debug purposes we allow non-square boards in certain situations

		if (last_alloc_wi == gtk_widget_get_allocated_width(board) && last_alloc_hi == gtk_widget_get_allocated_height(board)) {
			// NO CHANGE: return
			debug("No Change: last alloc wi %d hi %d\n", last_alloc_wi, last_alloc_hi);
			return FALSE;
		}

		// remember last alloc width
		last_alloc_wi = gtk_widget_get_allocated_width(board);
		last_alloc_hi = gtk_widget_get_allocated_height(board);

		if (first_configure) {
			needs_update = 1;
			first_configure = 0;
		} else {
			// invalidate old descale timer
			if (de_scale_timer) {
				g_source_remove(de_scale_timer);
			}
			needs_scale = 1;
			de_scale_timer = g_timeout_add(100, de_scale, board);
		}

		// Force app to repaint the whole board
		// as board will be scaled or updated
		gtk_widget_queue_draw(board);
	}

	return FALSE;
}

char theme_dir[] = "themes/commons/";
//char theme_dir[] = "themes/eyes/";
//char theme_dir[] = "themes/fantasy/";

static int load_piecesSvg() {

	int i;

	// relative or absolute
	char file_path[256];

	// get length of prefix
	int prefix_len = strlen(theme_dir);

	// prepare for strcat
	memset(file_path, 0 , sizeof(file_path));

	// append prefix
	strcat(file_path, theme_dir);

	// these must follow the order defined in the header
	char *suffixes[12] = {
		"wk.svg", 
		"wq.svg", 
		"wr.svg", 
		"wb.svg", 
		"wn.svg", 
		"wp.svg", 
		"bk.svg",
		"bq.svg", 
		"br.svg", 
		"bb.svg", 
		"bn.svg",
		"bp.svg", 
	};

	for (i=0; i<12; i++) {
		strcat(file_path, suffixes[i]);
		piecesSvg[i] = rsvg_handle_new_from_file (file_path, NULL);
		memset(file_path+prefix_len, 0, sizeof(file_path)-prefix_len);
	}

	return 0;
}

static int init_pieces(chess_game *game) {
	unsigned int i, j;

	for (i = 0; i < 8; i++) {
		game->white_set[i].type = W_PAWN;
		game->white_set[i].pos.row = 1;
		game->white_set[i].pos.column = i;
		game->squares[i][1].piece = &game->white_set[i];

		game->black_set[i].type = B_PAWN;
		game->black_set[i].pos.row = 6;
		game->black_set[i].pos.column = i;
		game->squares[i][6].piece = &game->black_set[i];
	}
	for (i = 8; i < 16; i++) {
		game->white_set[i].pos.row = 0;
		game->white_set[i].pos.column = i - 8;
		game->squares[i - 8][0].piece = &game->white_set[i];

		game->black_set[i].pos.row = 7;
		game->black_set[i].pos.column = i - 8;
		game->squares[i - 8][7].piece = &game->black_set[i];
	}

	game->white_set[ROOK1].type = W_ROOK;
	game->white_set[ROOK2].type = W_ROOK;
	game->white_set[BISHOP1].type = W_BISHOP;
	game->white_set[BISHOP2].type = W_BISHOP;
	game->white_set[KNIGHT1].type = W_KNIGHT;
	game->white_set[KNIGHT2].type = W_KNIGHT;
	game->white_set[QUEEN].type = W_QUEEN;
	game->white_set[KING].type = W_KING;

	game->black_set[ROOK1].type = B_ROOK;
	game->black_set[ROOK2].type = B_ROOK;
	game->black_set[BISHOP1].type = B_BISHOP;
	game->black_set[BISHOP2].type = B_BISHOP;
	game->black_set[KNIGHT1].type = B_KNIGHT;
	game->black_set[KNIGHT2].type = B_KNIGHT;
	game->black_set[QUEEN].type = B_QUEEN;
	game->black_set[KING].type = B_KING;

	for (i = 0; i < 16; i++) {
		game->white_set[i].dead = false;
		game->black_set[i].dead = false;
		game->white_set[i].colour = WHITE;
		game->black_set[i].colour = BLACK;
	}

	for (i = 0; i < 8; i++) {
		for (j = 2; j < 6; j++) {
			game->squares[i][j].piece = NULL;
		}
	}

	init_en_passant(game);
	init_castle_state(game);
	game->fifty_move_counter = 100;
	game->whose_turn = 0;

	init_hash(game);

	return 0;
}

static void reset_game(bool lock_threads) {
	main_game->current_move_number = 1;
	memset(main_game->moves_list, 0, strlen(main_game->moves_list));
	main_game->moves_list[0] = '\0';
	main_game->ply_num = 1;
	init_zobrist_hash_history(main_game);
	init_pieces(main_game);
	if (main_list != NULL) {
		plys_list_free(main_list);
	}
	main_list = plys_list_new();

	if (lock_threads) {
		gdk_threads_enter();
	}
	gtk_label_set_markup(GTK_LABEL(opening_code_label), "");
	gtk_widget_set_tooltip_text(opening_code_label, "");
	if (lock_threads) {
		gdk_threads_leave();
	}
}

int crafty_data_pipe[2];

static pthread_t move_event_processor_thread;

static void spawn_mover(void) {
	pthread_create(&move_event_processor_thread, NULL, process_moves, NULL);
}

void send_to_ics(char *s) {
	if (ics_mode) {
		size_t len = strlen(s);
		send_to_fics(s, &len);
	}
	else {
		debug("Would send to ICS %s", s);
	}
}

static bool uci_game_started = false;
void send_to_uci(char *s) {
	debug("Send user move to UCI %s", s);
	size_t len = strlen(s);
	s[len - 1] = '\0';
	// TODO: make it work in manual mode
//	if (!uci_game_started) {
//		uci_game_started = true;
//		start_new_uci_game(0, ENGINE_ANALYSIS);
//	}
	user_move_to_uci(s, true);
}

/* move must be a NULL terminated string */
int resolve_move(chess_game *game, int t, char *move, int resolved_move[4]) {

	int i, j, count;
	int ocol = -1, orow = -1;
	int ncol = -1, nrow = -1;

	int resolved = 0;

//	debug("Strlen (move) == %zd\n", strlen(move));
//	debug("type == %d\n", type);
//	debug("move == %s\n", move);

	if (strlen(move) == 2) {
		ncol = move[0] - 'a';
		nrow = move[1] - '1';
//		debug("ncol: %d - nrow: %d\n", ncol, nrow);
	} else if (strlen(move) >= 4) {
		ocol = move[0] - 'a';
		orow = move[1] - '1';
		ncol = move[2] - 'a';
		nrow = move[3] - '1';
//		debug("ocol %d - orow: %d - ncol: %d - nrow: %d\n", ocol, orow, ncol, nrow);
	}

	chess_piece *piece;
	int possible_moves[64][2];
	for (i = 0; i < 16; i++) {
		piece = &(game->whose_turn ? game->black_set : game->white_set)[i];
//		debug("Considering piece: %c%c%d - checking for dead %d - type %d , requested type %d\n", type_to_char(piece->type), 'a'+piece->pos.column, 1+piece->pos.row, piece->dead, piece->type, t);
		if (!piece->dead && piece->type == t) {
//			if (t == W_PAWN || t == B_PAWN) {
//				debug("Considering piece: %c%d - checking for dead %d - type %d , requested type %d\n", 'a'+piece->pos.column, 1+piece->pos.row, piece->dead, piece->type, t);
//			}
//			else {
//				debug("Considering piece: %c%c%d - checking for dead %d - type %d , requested type %d\n", type_to_char(piece->type), 'a'+piece->pos.column, 1+piece->pos.row, piece->dead, piece->type, t);
//			}
			if (ocol != -1 && ocol != piece->pos.column) {
				continue;
			}
			if (orow != -1 && orow != piece->pos.row) {
				continue;
			}
			count = get_possible_moves(game, piece, possible_moves, 1);
			for (j = 0; j < count; j++) {
				if (possible_moves[j][0] == ncol && possible_moves[j][1] == nrow) {
//					debug("Resolved Move: Piece set[%d] to %d,%d - checking legality...\n", i, ncol, nrow);
					if (is_move_legal(game, piece, ncol, nrow) ) {
//						debug("- Move legal [ok]\n");
						resolved = 1;
						break;
					}
//					else {
//						debug("Resolving Move: Piece set[%d] to %c%c%d\n", i, type_to_char(piece->type), 'a'+ncol, 1+nrow);
//						debug("- Move illegal [!!]\n");
//					}
				}
			}
		}
		if (resolved) break;
	}

	if (resolved) {
		resolved_move[0] = piece->pos.column;
		resolved_move[1] = piece->pos.row;
		resolved_move[2] = ncol;
		resolved_move[3] = nrow;
	}
	return resolved;
}

int open_file(const char *name) {
	debug("Loading '%s'\n", name);
	FILE *f = fopen( name, "r" );
	if (f == NULL) {
		fprintf(stderr, "Error opening file '%s': %s\n", name, strerror(errno));
		return 1;
	}
	san_scanner_in = f;
	san_scanner_restart(san_scanner_in);

	return 0;
}

void load_game(const char* file_path, int game_num) {

	if (open_file(file_path)) {
		return;
	}

	int i = 0;
	int games_counter = 0;

	gboolean inside_tags = FALSE;
	gboolean found_my_game = FALSE;
	gboolean failed = TRUE;

	gboolean blacks_ply = 0;

	while (i != -1) {
		i = san_scanner_lex();

		if (i == 2) {
			if (!inside_tags) {
				if (found_my_game) {
					break;
				}
				inside_tags = TRUE;
				games_counter++;
				debug("Found game %d\n", game_num);
				if (games_counter == game_num) {
					found_my_game = TRUE;
					failed = FALSE;
					debug("Found game %d\n", game_num);
				}
			}
			while (i == 2) {
				i = san_scanner_lex();
			}
		}
		if (i != -1) {
			if (inside_tags) {
				inside_tags = FALSE;
				if (found_my_game) {
					gdk_threads_enter();
					set_header_label(main_game->white_name, main_game->black_name, main_game->white_rating, main_game->black_rating);
					gdk_threads_leave();
					if (main_list != NULL) {
						plys_list_free(main_list);
					}
					main_list = plys_list_new();
				}
			}
			if (found_my_game) {
				debug("raw move %c%s - whose_turn %d\n", type_to_char(type), currentMoveString, blacks_ply);
				type = colorise_type(type, blacks_ply);
				int resolved = resolve_move(main_game, type, currentMoveString, resolved_move);
				if (resolved) {
					debug("move resolved to %c%d-%c%d\n", resolved_move[0]+'a', resolved_move[1]+1, resolved_move[2]+'a', resolved_move[3]+1);
					char san[SAN_MOVE_SIZE];
					move_piece(main_game->squares[resolved_move[0]][resolved_move[1]].piece, resolved_move[2], resolved_move[3], 0, AUTO_SOURCE_NO_ANIM, san, main_game, false);
					plys_list_append_ply(main_list, ply_new(resolved_move[0], resolved_move[1], resolved_move[2], resolved_move[3], NULL, san));
					blacks_ply = ! blacks_ply;
				}
				else {
					fprintf(stderr, "Could not resolve move !\n");
					failed = TRUE;
					break;
				}
			}
		}
	}
	if (!failed) {
		debug("Successfully parsed game number '%d' in database '%s'\n", game_num, file_path);
		refresh_moves_list_view(main_list);
	}
	else {
		fprintf(stderr, "Failed to load/parse game number '%d' in database '%s'\n", game_num, file_path);
	}
}

/* replays from scratch the plys in list on the passed squares and pieces.
 * NOTE: the passed squares will be reset */
int replay_moves_list_from_scratch(plys_list *list, chess_square sq[8][8], chess_piece w_set[16], chess_piece b_set[16]) {
	return 0;
}

struct timeval wait_until_time;

gboolean auto_play_one_move(gpointer data) {

	static int waiting = 0;

	struct timeval current_time;
	gettimeofday(&current_time, NULL);
	if (wait_until_time.tv_sec > current_time.tv_sec) {
		return TRUE;
	}
	else if (wait_until_time.tv_sec == current_time.tv_sec &&
			wait_until_time.tv_usec > current_time.tv_usec) {
		return TRUE;
	}

	int i;

	if (!is_running_flag()) {
		return FALSE;
	}

	i = san_scanner_lex();
	if (i == 2 || i == MATCHED_END_TOKEN) {
		if (waiting) {
			debug("In if waiting\n");
			waiting = 0;
			reset_game(true);
			memset(main_game->white_name, 0, 256);
			memset(main_game->black_name, 0, 256);
		}
		if (playing) {
			debug("In if playing\n");
			playing = false;
			if (i == MATCHED_END_TOKEN) {
				char bufstr[33];
				if (!main_game->whose_turn) {
					snprintf(bufstr, 33, "\t%s", san_scanner_text);
				}
				else {
					strncpy(bufstr, san_scanner_text, 32);
				}
				insert_text_moves_list_view(bufstr, true);
			}
			end_game();
			waiting = 1;
			wait_until_time.tv_sec = current_time.tv_sec + auto_play_delay / 1000;
			wait_until_time.tv_usec = current_time.tv_usec + (auto_play_delay * 1000) % 1000000;
			return TRUE;
		}

		while (i == 2) {
			i = san_scanner_lex();
		}
	}
	if (i != -1) {
		if (!playing) {
			playing = true;
			start_game(main_game->white_name, main_game->black_name, 0, 0, -2, true);
			start_new_uci_game(0, ENGINE_ANALYSIS);
			if (!strncmp("Kasparov, Gary", main_game->black_name, 16)) {
				g_signal_emit_by_name(board, "flip-board");
			}
		}
		int resolved = resolve_move(main_game, type, currentMoveString, resolved_move);
		if (resolved) {
			auto_move(main_game->squares[resolved_move[0]][resolved_move[1]].piece, resolved_move[2], resolved_move[3], 0, AUTO_SOURCE, false);
			return TRUE;
		} else {
			fprintf(stderr, "Could not resolve move %c%s\n", type_to_char(type), currentMoveString);
		}
	}

	// Reached EOF
	playing = false;
	return FALSE;
}

gboolean print_main_clock(gpointer data) {
	print_clock(main_clock);
	return TRUE;
}


gboolean auto_play_one_ics_move(gpointer data) {
	debug("Autoplay one ics move\n");
	int resolved_move[4];
	char lm[MOVE_BUFF_SIZE];

	get_last_move(lm);
	san_scanner__scan_string(lm);

	if (san_scanner_lex() != -1) {
		playing = true;
		char type_char = type_to_char(type);
		if (!type_char) {
			debug("Raw Move %s\n", currentMoveString);
		} else {
			debug("Raw Move %c%s\n", type_char, currentMoveString);
		}
		int resolved = resolve_move(main_game, type, currentMoveString, resolved_move);
		if (resolved) {
			debug("Move resolved to %c%d-%c%d\n", resolved_move[0] + 'a', resolved_move[1] + 1, resolved_move[2] + 'a', resolved_move[3] + 1);
			auto_move(main_game->squares[resolved_move[0]][resolved_move[1]].piece, resolved_move[2], resolved_move[3], 0, AUTO_SOURCE, false);
			check_board12_game_consistency();
			int premove[4];
			get_pre_move(premove);
			if (premove[0] != -1) {
				if (auto_move(main_game->squares[premove[0]][premove[1]].piece, premove[2], premove[3], 1, AUTO_SOURCE, false) > 0) {
					printf("Pre-move was accepted\n");
					unset_pre_move();
				} else {
					printf("Pre-move was rejected\n");
				}
			}
			return true;
		} else {
			fprintf(stderr, "Could not resolve move %c%s\n", type_to_char(type), currentMoveString);
		}
	} else {
		fprintf(stderr, "san_scanner_lex returned -1 while scanning last move '%s'\n", lm);
	}

	// Reached EOF
	playing = false;
	return false;
}

gboolean auto_play_one_crafty_move(gpointer data) {
	debug("Autoplay one crafty move\n");
	int i;
	int resolved_move[4];
	char lm[MOVE_BUFF_SIZE];

	get_last_move(lm);
	san_scanner__scan_string(lm);
	i = san_scanner_lex();

	if ( i != -1) {
		playing = true;
		char ctype = type_to_char(type);
		if (!ctype) {
			debug("Raw Move %s\n", currentMoveString);
		}
		else {
			debug("Raw Move %c%s\n", ctype, currentMoveString);
		}
		int resolved = resolve_move(main_game, type, currentMoveString, resolved_move);
		if (resolved) {
			char *ics_command = calloc(16, sizeof(char));
			snprintf(ics_command, 16, "%c%d%c%d", resolved_move[0]+'a', resolved_move[1]+1, resolved_move[2]+'a', resolved_move[3]+1);

			/* hack for sending promote to ics */
			char *promo = strchr(lm, '=');
			if (promo) {
				strcat(ics_command, promo);
			}

			ics_command[strlen(ics_command)] = '\n';

			send_to_ics(ics_command);
			debug("Crafty Move resolved to %s", ics_command);

			free(ics_command);

			auto_move(main_game->squares[resolved_move[0]][resolved_move[1]].piece, resolved_move[2], resolved_move[3], 0, AUTO_SOURCE, false);
			return TRUE;
		}
		else {
			fprintf(stderr, "Could not resolve move %c%s\n", type_to_char(type), currentMoveString);
		}
	}
	else {
		fprintf(stderr, "san_scanner_lex returned -1\n");
	}

	// Reached EOF
	playing = false;
	return FALSE;
}

gboolean auto_play_one_uci_move(gpointer data) {
	debug("Autoplay one UCI move\n");
	int resolved_move[4];
	char lm[MOVE_BUFF_SIZE];

	get_last_move(lm);
	debug("lm %c%c %c%c\n", lm[0], lm[1], lm[2], lm[3]);
	resolved_move[0] = lm[0] - 'a';
	resolved_move[1] = lm[1] - '1';
	resolved_move[2] = lm[2] - 'a';
	resolved_move[3] = lm[3] - '1';
	debug("resolved_move %d%d %d%d\n", resolved_move[0], resolved_move[1], resolved_move[2], resolved_move[3]);

	auto_move(main_game->squares[resolved_move[0]][resolved_move[1]].piece, resolved_move[2], resolved_move[3], 0, AUTO_SOURCE, false);
	return true;
}

// handle max of 10 starmatches at once (per-looking at)
char star_match[10][1024];

/* *
 * Test whether pattern is present at &buf[*index]; if so, return TRUE,
 * advance *index beyond it; else return FALSE.
 * If pattern contains the character '*', it matches any sequence of
 * characters not containing '\r', '\n', or the character following
 * the '*' (if any), and the matched sequence(s) are copied into star_match.
 * NOTE: star_match will be NULL terminated
 * */
int looking_at(char *buf, int *index, char *pattern) {
	int star_index = 0;
	char *bufp = &buf[*index], *patternp = pattern;
	char *matchp = star_match[star_index];

	for (;;) {
		if (*patternp == 0) {
			*index = bufp - buf;
			*matchp = 0;
			return TRUE;
		}
		if (*bufp == 0) {
			return FALSE;
		}
		if (*patternp == '*') { // inside star match
			if (*bufp == *(patternp + 1)) { // matched character after * - exit star match
				*matchp = 0;
				matchp = star_match[++star_index];
				patternp += 2;
				bufp++;
				continue;
			}
			else if (*bufp == '\n' || *bufp == '\r') { // matched line ending - exit star match
				patternp++;
				if (*patternp == 0) {
					continue;
				}
				else {
				  return FALSE;
				}
			}
			else { // still inside star match
				*matchp++ = *bufp++;
				continue;
			}
		}
		if (*patternp != *bufp) {
			return FALSE;
		}
		patternp++;
		bufp++;
	}
}

char login_name[112];
char player_vs[128];
char vs_player[128];
//int ics_game_started;

void set_header_label(const char *w_name, const char *b_name, const char *w_rating, const char *b_rating) {
	char header[256];
	char part1[128];
	char part2[128];

	memset(header, 0, sizeof(header));
	memset(part1, 0, sizeof(part1));
	memset(part2, 0, sizeof(part2));

	if (w_rating != NULL && w_rating[0] != 0) {
		snprintf(part1, sizeof(part1)-1, "%s (%s)", w_name, w_rating);
	}
	else {
		strncpy(part1, w_name, sizeof(part1)-1);
	}
	if (b_rating != NULL && b_rating[0] != 0) {
		snprintf(part2, sizeof(part2)-1, "%s (%s)", b_name, b_rating);
	}
	else {
		strncpy(part2, b_name, sizeof(part2)-1);
	}

	snprintf(header, sizeof(header)-1, "%s\nvs\n%s", part1, part2);

	gtk_label_set_text(GTK_LABEL(moves_list_title_label), header);

}

void start_game(char *w_name, char *b_name, int seconds, int increment, int relation, bool should_lock) {

	clock_reset(main_clock, seconds, increment, relation, should_lock);

	if (should_lock) {
		gdk_threads_enter();
	}

	game_started = true;

	reset_game(false);
	reset_moves_list_view(FALSE);

	reset_board();

	/* set window title with players' names */
	if (*w_name && *b_name) {
		char header[256];
		sprintf(header, "%s\nvs\n%s", w_name, b_name);
		gtk_label_set_text(GTK_LABEL(moves_list_title_label), header);

		char buff[256];
		char title[256];
		sprintf(buff, "%s vs %s", w_name, b_name);
		sprintf(title, "cairo-board - %s", buff);
		gtk_window_set_title(GTK_WINDOW(main_window), title);
	}

	if (should_lock) {
		gdk_threads_leave();
	}

	switch (relation) {
		case 1:
			debug("Game started as White\n");
			game_mode = I_PLAY_WHITE;
			break;
		case -1:
			debug("Game started as Black\n");
			g_signal_emit_by_name(board, "flip-board");
			game_mode = I_PLAY_BLACK;
			break;
		case 0:
			debug("Observing played game\n");
			game_mode = I_OBSERVE;
			break;
		case -2:
			debug("Observing examined game\n");
			game_mode = I_OBSERVE;
			break;
		case -3:
			game_mode = MANUAL_PLAY;
			break;
		default:
			debug("Unknown relation %d\n", relation);
	}
}

void end_game(void) {
	game_started = false;
	clock_started = 0;
	my_game = 0;
	clock_freeze(main_clock);
}

gint cleanup(gpointer ignored) {

	debug("Starting cleanup phase...\n");

	// stop autoplay
	if (auto_play_timer) {
		g_source_remove(auto_play_timer);
	}

	// stop clocks
	if (clock_refresher) {
		g_source_remove(clock_refresher);
	}

	/* set global running flag off */
	set_running_flag(false);

	/* close socket */
//	if (ics_open_flag) {
//		ics_open_flag = false;
//		close_tcp(ics_fd);
//	}

	/* cancel threads and wait for all threads to exit */
	debug("Cancelling all threads...\n");
	pthread_cancel(move_event_processor_thread);
	pthread_join(move_event_processor_thread, NULL);
	if (ics_mode) {
		cleanup_ics();
	}

	cleanup_uci();
	cleanup_mutexes();

	debug("All threads terminated\n");

	debug("Finished cleanup\n");

	return FALSE;
}

wint_t type_to_unicode_char(int type) {
	return (wint_t) BASE_CHESS_UNICODE_CHAR + (invert_fig_colours ? (type + 6) % 12 : type);
}

/* <Moves List data structures utilities> */
ply *ply_new(int oc, int or, int nc, int nr, chess_piece *taken, const char *san) {
	ply *new;
	new = malloc(sizeof(ply));
	new->old_col = oc;
	new->old_row = or;
	new->new_col = nc;
	new->new_row = nr;
	new->piece_taken = taken;
	strncpy(new->san_string, san, 15);
	return new;
}

void ply_print(ply *to_print) {
		printf("\tply_number: %d\n", to_print->ply_number);
		printf("\told col: %d\n", to_print->old_col);
		printf("\told row: %d\n", to_print->old_row);
		printf("\tnew col: %d\n", to_print->new_col);
		printf("\tnew row: %d\n", to_print->new_row);
		printf("\tPiece was %s\n", (to_print->piece_taken == NULL ? "NOT taken" : "taken") );
		printf("\tSAN move %s\n", to_print->san_string);
}

plys_list *plys_list_new(void) {
	
	plys_list *new;

	new = malloc(sizeof(plys_list));
	new->plys = calloc(MOVES_LIST_ALLOC_PAGE_SIZE, sizeof(ply*));

	new->last_ply = 0;
	new->viewed_ply = 0;
	new->plys_allocated = MOVES_LIST_ALLOC_PAGE_SIZE;

	return new;
}

// Grows the plys_list allocated memory region by a factor of MOVES_LIST_ALLOC_PAGE_SIZE
void plys_list_grow(plys_list *list) {
	debug("Growing Moves List!!\n");

	/* grow the allocated memory */
	list->plys = realloc(list->plys, 
			sizeof(ply) * (MOVES_LIST_ALLOC_PAGE_SIZE + list->plys_allocated) );

	/* initialising newly allocated memory to 0 */
	memset(list->plys+(list->plys_allocated), 0, sizeof(ply)*MOVES_LIST_ALLOC_PAGE_SIZE);

	/* updating allocated counter */
	list->plys_allocated += MOVES_LIST_ALLOC_PAGE_SIZE;
}

void plys_list_append_ply(plys_list *list, ply *to_append) {
	if (list->last_ply >= list->plys_allocated - 1) {
		plys_list_grow(list);
	}

	/* sets the half move number */
	to_append->ply_number = list->last_ply + 1;

	list->plys[list->last_ply++] = to_append;
}

void plys_list_print(plys_list *list) {
	printf("Printing moves list:\n");
	int i = 0;
	while (list->plys[i] != NULL) {
		ply_print(list->plys[i]);
		i++;
	}
}

void plys_list_free(plys_list *to_destroy) {
	int i = 0;
	while(to_destroy->plys[i] != NULL) {
		free(to_destroy->plys[i]);
		i++;
	}
	free(to_destroy->plys);
	free(to_destroy);
}
/* </Moves List data structures utilities> */

/* Append text at the end of the moves_list buffer
 * NB: text must be NULL terminated*/
void insert_text_moves_list_view(const gchar *text, bool should_lock_threads) {
	if (should_lock_threads) {
		gdk_threads_enter();
	}
	if (!GTK_IS_TEXT_VIEW(moves_list_view)) {
		// Killed? tough!
		return;
	}

	/* append the text at the end of the buffer */
	GtkTextMark *end_mark = gtk_text_buffer_get_mark(moves_list_buffer, "end_bookmark");
	if (!end_mark) {
		fprintf(stderr, "Failed to get the mark at the end of the moves_list buffer!\n");
		return;
	}
	GtkTextIter mark_it;
	gtk_text_buffer_get_iter_at_mark(moves_list_buffer, &mark_it, end_mark);
	gtk_text_buffer_insert(moves_list_buffer, &mark_it, text, -1);

	/* scroll to bottom */
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(moves_list_view), end_mark, .0, FALSE, .0, .0);

	if (should_lock_threads) {
		gdk_threads_leave();
	}
}


/* maps white_pawn type and black_pawn type to <colour>_pawn type etc... */
int get_type_colour(int tt) {
	if (tt < B_KING) {
		return 0;
	}
	return 1;
}

/* converts white pawn type to black pawn type etc... */
int swap_type_colour(int tt) {
	if (get_type_colour(tt)) {
		return tt-B_KING;
	}
	return tt+B_KING;
}

/* maps white_pawn type and black_pawn type to <colour>_pawn type etc... */
int colorise_type(int tt, int colour) {
	if (get_type_colour(tt) != colour) {
		return swap_type_colour(tt);
	}
	return tt;
}


/* delete contents of the moves list view and 
 * repopulate it with the passed plys_list */
void refresh_moves_list_view(plys_list *list) {

	int ply_colour;

	char *str;
	str = calloc(list->last_ply, 16*sizeof(char));

	char str1[32];
	memset(str1, 0, sizeof(str));
	char str2[32];
	memset(str2, 0, sizeof(str));

	reset_moves_list_view(TRUE);

	int i = 0;
	while (list->plys[i] != NULL) {
		
		ply *p = list->plys[i];
		ply_colour = (p->ply_number + 1) % 2; // remember plys start at 1

		char pchar = p->san_string[0];
		int tt = char_to_type(main_game->whose_turn, pchar);
		if (use_fig && tt != -1) {
			tt = colorise_type(tt, ply_colour);
			sprintf(str1, "%lc", type_to_unicode_char(tt));
			strcat(str1, p->san_string+1);
		}
		else {
			strcpy(str1, p->san_string);
		}

		/* insert move number if it is white's ply */
		if (!ply_colour) {
			sprintf(str2, "%d.\t", 1+p->ply_number/2);
			strcat(str, str2);
			strcat(str, str1);
			strcat(str, "\t");
		}
		else {
			/* append line feed if we're printing black's move */
			sprintf(str2, "%s\n", str1);
			strcat(str, str2);
		}


		i++;
	}
	insert_text_moves_list_view(str, true);

	free(str);
}


void insert_san_move(const char* san_move, bool should_lock_threads) {

	append_san_move(main_game, san_move);

	char str[32];
	memset(str, 0, sizeof(str));
	char buf_str[16];
	memset(buf_str, 0, sizeof(buf_str));

	char pchar = san_move[0];
	int tt = char_to_type(main_game->whose_turn, pchar);
	if (use_fig && tt != -1) {
		// char_to_type() will return opposite colour because we swapped whose_turn already
		tt = colorise_type(tt, !main_game->whose_turn);
		sprintf(buf_str, "%lc", type_to_unicode_char(tt));
		strcat(buf_str, san_move + 1);
	} else {
		strcpy(buf_str, san_move);
	}
	char *promo = strrchr(san_move, '=');
	if (use_fig && promo != NULL) {
		// promo handling figurine
		int promo_type = char_to_type(!main_game->whose_turn, promo[1]);
		sprintf(promo + 1, "%lc", type_to_unicode_char(promo_type));
	}

	/* insert move number if it *was* white's ply */
	if (main_game->whose_turn) {
		sprintf(str, "%d.\t", main_game->current_move_number);
		strcat(str, buf_str);
		strcat(str, "\t");
	}
	else {
		/* append line feed if we're printing black's move */
		sprintf(str, "%s\n", buf_str);
	}

	insert_text_moves_list_view(str, should_lock_threads);

}

/* deletes the contents of the buffer associated with the moves list view */
void reset_moves_list_view(gboolean should_lock_threads) {
	if (should_lock_threads) {
		gdk_threads_enter();
	}
	gtk_text_buffer_set_text(moves_list_buffer, "", -1);
	if (should_lock_threads) {
		gdk_threads_leave();
	}
}

GHashTable *eco_full;

#define ECO_LINE_MAX 256

int compile_eco(void) {
	char name[] = "full_eco.idx";
	char san_key[ECO_LINE_MAX];
	char full_description[ECO_LINE_MAX];

	eco_full = g_hash_table_new(g_str_hash, g_str_equal);
	FILE *f = fopen(name, "r");
	if (f == NULL) {
		fprintf(stderr, "Error opening file '%s': %s\n", name, strerror(errno));
		return 1;
	}
	while (fgets(san_key, ECO_LINE_MAX, f) != NULL) {
		san_key[strlen(san_key) - 1] = 0;
//		printf("San Key '%s'\n", san_key);
		if (fgets(full_description, ECO_LINE_MAX, f)) {
			full_description[strlen(full_description) - 1] = 0;
//			printf("Full_description '%s'\n", full_description);
			g_hash_table_insert(eco_full, strdup(san_key), strdup(full_description));
		}
	}

	return 0;
}

char *get_eco_full(const char *san_moves_list) {
	return g_hash_table_lookup(eco_full, san_moves_list);
}

static void get_theme_colours(GtkWidget *widget) {
	GdkRGBA fg_color;
	GdkRGBA bg_color;
	GtkStyleContext *context = gtk_widget_get_style_context(widget);

	if (context != NULL) {
		gtk_style_context_lookup_color(context, "theme_bg_color", &bg_color);
		gtk_style_context_lookup_color(context, "theme_fg_color", &fg_color);
		chat_handle_colour.red = (fg_color.red + bg_color.red) / 2.0;
		chat_handle_colour.green = (fg_color.green + bg_color.green) / 2.0;
		chat_handle_colour.blue = (fg_color.blue + bg_color.blue) / 1.85;
		chat_handle_colour.alpha = 1.0;
		double bg_lum = bg_color.red + bg_color.green + bg_color.blue;
		double fg_lum = fg_color.red + fg_color.green + fg_color.blue;
		invert_fig_colours = bg_lum < fg_lum;
	} else {
		chat_handle_colour.red = 0.5;
		chat_handle_colour.green = 0.5;
		chat_handle_colour.blue = 0.5;
		chat_handle_colour.alpha = 1.0;
		invert_fig_colours = false;
	}
}

static gboolean spawn_uci_engine_idle(gpointer data) {
	debug("Spawning UCI engine...\n");
	bool brainfish = *(bool*)data;
	spawn_uci_engine(brainfish);
	debug("Spawned UCI engine [OK]\n");
	if (!ics_mode) {
		debug("Starting new UCI game...\n");
//		start_new_uci_game(60, ENGINE_WHITE);
//		start_new_uci_game(60, ENGINE_BLACK);
		start_new_uci_game(60, ENGINE_ANALYSIS);
		start_uci_analysis();
	}
	return FALSE;
}

int main (int argc, char **argv) {

	int c;
	static struct option long_options[] = {
			{"debug",      no_argument,       &debug_flag,         TRUE},
			{"d",          no_argument,       &debug_flag,         TRUE},
			{"first",      no_argument,       &test_first_player,  TRUE},
			{"login1",     required_argument, 0,                   ICS_TEST_HANDLE1},
			{"login2",     required_argument, 0,                   ICS_TEST_HANDLE2},
			{"ics",        no_argument,       &ics_mode,           TRUE},
			{"guest",      no_argument,       &guest_mode,         true},
			{"fig",        no_argument,       &use_fig,            TRUE},
			{"icshost",    required_argument, 0,                   ICS_HOST_ARG},
			{"icsport",    required_argument, 0,                   ICS_PORT_ARG},
			{"lr",         required_argument, 0,                   LIGHT_RED_ARG},
			{"lg",         required_argument, 0,                   LIGHT_GREEN_ARG},
			{"lb",         required_argument, 0,                   LIGHT_BLUE_ARG},
			{"dr",         required_argument, 0,                   DARK_RED_ARG},
			{"dg",         required_argument, 0,                   DARK_GREEN_ARG},
			{"db",         required_argument, 0,                   DARK_BLUE_ARG},
			{"load",       required_argument, 0,                   LOAD_FILE_ARG},
			{"gamenum",    required_argument, 0,                   LOAD_GAME_NUM_ARG},
			{"delay",      required_argument, 0,                   AUTO_PLAY_DELAY_ARG},
			{0,            0,                 0,                   0}
	};

	opterr = 1;
	optind = 1;
	for(;;) {
		// getopt_long stores the option index here
		int option_index = 0;

		c = getopt_long_only (argc, argv, "", long_options, &option_index);

		// Detect the end of the options
		if (c == -1) {
			break;
		}

		switch (c) {
			case ICS_HOST_ARG:
				ics_host_specified = true;
				strncpy(ics_host, optarg, sizeof(ics_host));
				break;
			case ICS_TEST_HANDLE1:
				ics_handle1_specified = true;
				strncpy(ics_handle1, optarg, sizeof(ics_handle1));
				break;
			case ICS_TEST_HANDLE2:
				ics_handle2_specified = true;
				strncpy(ics_handle2, optarg, sizeof(ics_handle2));
				break;
			case ICS_PORT_ARG:
				ics_port_specified = true;
				ics_port = (unsigned short) atoi(optarg);
				break;
			case LIGHT_RED_ARG:
				lr = (double) atoi(optarg)/255.0;
				break;
			case LIGHT_GREEN_ARG:
				lg = (double) atoi(optarg)/255.0;
				break;
			case LIGHT_BLUE_ARG:
				lb = (double) atoi(optarg)/255.0;
				break;
			case DARK_RED_ARG:
				dr = (double) atoi(optarg)/255.0;
				break;
			case DARK_GREEN_ARG:
				dg = (double) atoi(optarg)/255.0;
				break;
			case DARK_BLUE_ARG:
				db = (double) atoi(optarg)/255.0;
				break;
			case LOAD_FILE_ARG:
				load_file_specified = true;
				strncpy(file_to_load, optarg, sizeof(file_to_load));
				break;
			case LOAD_GAME_NUM_ARG:
				game_to_load = (unsigned int) atoi(optarg);
				break;
			case AUTO_PLAY_DELAY_ARG:
				auto_play_delay = atoi(optarg);
				break;

			default:
				break;

		}
	}

	// Compute highlight colours
	highlight_selected_r = 1;
	highlight_selected_g = (dg + lg) / 3.0;
	highlight_selected_b = (db + lb) / 3.0;
	highlight_selected_a = 0.3;

	highlight_move_r = (dr + lr) / 3.0;
	highlight_move_g = 1;
	highlight_move_b = (db + lb) / 3.0;
	highlight_move_a = 0.3;

	highlight_pre_move_r = 1;
	highlight_pre_move_g = (dg + lg) / 3.0;
	highlight_pre_move_b = (db + lb) / 3.0;
	highlight_pre_move_a = 0.5;

	debug("Debug info enabled\n");
	if (ics_mode) {
		if (!ics_host_specified) {
			debug("ICS mode enabled but no host specified, will use default host %s\n", default_ics_host);
			strncpy(ics_host, default_ics_host, sizeof(ics_host));
		}
		if (!ics_port_specified) {
			debug("ICS mode enabled but no port specified, will use default port %u\n", default_ics_port);
			ics_port = default_ics_port;
		}
		debug("ICS mode enabled, will connect to %s on port %d\n", ics_host, ics_port);
	}

	/* if the user requested unicode figurines, check we can actually print them */
	if (use_fig) {
		/* set LC_CTYPE from the environment variable */
		if (!setlocale(LC_CTYPE, "")) {
			fprintf(stderr, "Figurines notation enabled but can't set the LC_CTYPE locale! "
					"Check LANG, LC_CTYPE, LC_ALL.\n");
			return 1;
		}

		/* try C's multibyte output mechanism */
		if (printf("%lc", BASE_CHESS_UNICODE_CHAR) < 0) {
			/* it failed, probably because of bad LC_CTYPE? */
			fprintf(stderr, "Failed to print UTF-8 multibyte character! "
					"Check LANG, LC_CTYPE, LC_ALL.\n"
					"Disabling figurines notations as they might not work.\n"
				   );
			/* disable figurines in case they don't work */
			use_fig = FALSE;
		}
		else { // delete the character if we successfully printed it
			printf("\r \r");
			fflush(stdout);
		}

	}

	init_config();
	compile_eco();

	old_wi = old_hi = 0;
	int win_def_wi;
	int win_def_hi;

	win_def_wi = 1338;
	win_def_hi = 950;

	create_signals();

	// install signal handler for SIGABRT and SIGSEGV
	signal(SIGABRT, sig_handler);
	signal(SIGSEGV, sig_handler);
	signal(SIGINT, sig_handler);

	/* initialise random numbers for Zobrist hashing */
	init_zobrist_keys();

	init_clock_colours();

	init_anims_map();

	/* Initilialise threading stuff */
	gdk_threads_init();
	gdk_threads_enter();

	gtk_init(&argc, &argv);

	load_piecesSvg();

	main_game = game_new();

	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(main_window, "map-event", G_CALLBACK(get_theme_colours), NULL);

	// CSS
	GtkCssProvider *provider = gtk_css_provider_new();
	GdkDisplay *display = gdk_display_get_default();
	GdkScreen *screen = gdk_display_get_default_screen(display);
	gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	const char *css_file = "cairo-board.css";
	GError *css_error = NULL;
	gtk_css_provider_load_from_file(provider, g_file_new_for_path(css_file), &css_error);
	if (css_error != NULL) {
		debug("Loading CSS Error! %s\n", css_error->message);
	} else {
		debug("Loaded CSS\n");
	}
	g_object_unref(provider);

	/* the central split pane */
	GtkWidget *split_pane;
	split_pane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

	/* moves list text view widget */
	moves_list_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(moves_list_view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(moves_list_view), FALSE);

	moves_list_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW (moves_list_view));

	PangoFontDescription *desc;
	desc = pango_font_description_from_string("Sans 12");
	gtk_widget_modify_font(moves_list_view, desc);

	/* get the average width of a SAN char in pango units */
	char san_chars[] = "NBQKabcdefgh12345678x=+#-*O";
	int san_char_width;
	PangoLayout *playout = gtk_widget_create_pango_layout(moves_list_view, san_chars);
	pango_layout_get_size(playout, &san_char_width, NULL);
	g_object_unref(playout);
	san_char_width /= strlen(san_chars);

	/* set tab stop */
	PangoTabArray* tabarray = pango_tab_array_new_with_positions(2, 0, PANGO_TAB_LEFT, 6 * san_char_width, PANGO_TAB_LEFT, 15 * san_char_width);
	gtk_text_view_set_tabs(GTK_TEXT_VIEW(moves_list_view), tabarray);
	pango_tab_array_free(tabarray);

	/* Title label for moves list viewer */
	moves_list_title_label = gtk_label_new("\nCairo-Board\n");
	gtk_label_set_justify(GTK_LABEL(moves_list_title_label), GTK_JUSTIFY_CENTER);
	desc = pango_font_description_from_string ("Sans Bold 12");
	gtk_widget_modify_font(moves_list_title_label, desc);

	/* Frame for Title label */
	label_frame_event_box = gtk_event_box_new();
	label_frame = gtk_frame_new(NULL);
	GtkWidget *align = gtk_alignment_new(.5, .5, 0, 0);
	gtk_container_add (GTK_CONTAINER (align), moves_list_title_label);
	gtk_container_add (GTK_CONTAINER (label_frame), align);
	gtk_container_add (GTK_CONTAINER (label_frame_event_box), label_frame);

	/* controls for displayed ply */
//	play_pause_button = gtk_button_new();
//	g_object_set(play_pause_button, "can-focus", FALSE, NULL);
//	gtk_widget_set_tooltip_text(play_pause_button, "Play/Pause");
//	gtk_button_set_image(GTK_BUTTON(play_pause_button),
//	                     (gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_SMALL_TOOLBAR)));

	goto_first_button = gtk_button_new();
	g_object_set(goto_first_button, "can-focus", FALSE, NULL);
	gtk_widget_set_tooltip_text(goto_first_button, "Show first move");
	gtk_button_set_image(GTK_BUTTON(goto_first_button),
	                     (gtk_image_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS, GTK_ICON_SIZE_SMALL_TOOLBAR)));

	goto_last_button = gtk_button_new();
	g_object_set(goto_last_button, "can-focus", FALSE, NULL);
	gtk_widget_set_tooltip_text(goto_last_button, "Show last move");
	gtk_button_set_image(GTK_BUTTON(goto_last_button),
	                     (gtk_image_new_from_stock(GTK_STOCK_MEDIA_NEXT, GTK_ICON_SIZE_SMALL_TOOLBAR)));

	go_back_button = gtk_button_new();
	g_object_set(go_back_button, "can-focus", FALSE, NULL);
	gtk_widget_set_tooltip_text(go_back_button, "Show previous move");
	gtk_button_set_image(GTK_BUTTON(go_back_button),
	                     (gtk_image_new_from_stock(GTK_STOCK_MEDIA_REWIND, GTK_ICON_SIZE_SMALL_TOOLBAR)));

	go_forward_button = gtk_button_new();
	g_object_set(go_forward_button, "can-focus", FALSE, NULL);
	gtk_widget_set_tooltip_text(go_forward_button, "Show next move");
	gtk_button_set_image(GTK_BUTTON(go_forward_button),
	                     (gtk_image_new_from_stock(GTK_STOCK_MEDIA_FORWARD, GTK_ICON_SIZE_SMALL_TOOLBAR)));

	GtkWidget *controls_h_box = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(controls_h_box), goto_first_button, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(controls_h_box), go_back_button, TRUE, TRUE, 0);
//	gtk_box_pack_start(GTK_BOX(controls_h_box), play_pause_button, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(controls_h_box), go_forward_button, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(controls_h_box), goto_last_button, TRUE, TRUE, 0);

	/* scrolled window for moves list */
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(scrolled_window), moves_list_view);

	/* right gravity */
	GtkTextIter start_bozo, end_iter;
	gtk_text_buffer_get_bounds(moves_list_buffer, &start_bozo, &end_iter);

	/* create the end mark */
	gtk_text_buffer_create_mark(moves_list_buffer, "end_bookmark", &end_iter, 0);

	gtk_text_buffer_create_tag(moves_list_buffer, "underline", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag(moves_list_buffer, "center", "justification", GTK_JUSTIFY_CENTER, NULL);
	gtk_text_buffer_create_tag(moves_list_buffer, "font10pt", "size-points", 10.0, NULL);

	/* Opening code label */
	opening_code_label = gtk_label_new("");
	add_class(opening_code_label, "eco-label");
	gtk_misc_set_alignment(GTK_MISC(opening_code_label), 0, .5);
	GtkWidget *opening_code_frame = gtk_frame_new(NULL);
	gtk_container_add(GTK_CONTAINER (opening_code_frame), opening_code_label);
	GtkWidget *opening_code_frame_event_box = gtk_event_box_new();
	gtk_container_add (GTK_CONTAINER (opening_code_frame_event_box), opening_code_frame);

	/* vbox to pack title label, controls and view area*/
	GtkWidget *moves_v_box = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(moves_v_box), label_frame_event_box, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(moves_v_box), controls_h_box, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(moves_v_box), scrolled_window, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(moves_v_box), opening_code_frame_event_box, FALSE, FALSE, 0);
	gtk_widget_set_size_request(moves_v_box, 350, -1);

	/* create the board area */
	board = gtk_drawing_area_new();

	gtk_window_set_default_size(GTK_WINDOW(main_window), win_def_wi, win_def_hi);
	gtk_window_set_icon_from_file(GTK_WINDOW(main_window), "icon.png", NULL);

	/* The board container */
	GtkWidget *board_frame;
	board_frame = gtk_aspect_frame_new(NULL, 0, 0, 1.0f, FALSE);
	gtk_widget_set_size_request(board_frame, 256, 256);
	gtk_frame_set_shadow_type(GTK_FRAME(board_frame), GTK_SHADOW_NONE);

	/* The clock container */
	GtkWidget *clock_frame;
	clock_frame = gtk_frame_new(NULL);
	gtk_widget_set_size_request(clock_frame, -1, 32);
	gtk_frame_set_shadow_type(GTK_FRAME(clock_frame), GTK_SHADOW_NONE);

	/* allocate the main clock */
	main_clock = clock_new(0, 0, 0);

	if (!main_clock) {
		fprintf(stderr, "Alloc failed??\n");
	}
	/* The clock widget */
	clock_widget = clock_face_new();
	clock_face_set_clock( CLOCK_FACE(clock_widget), main_clock);

	gtk_container_add (GTK_CONTAINER (board_frame), board);
	gtk_container_add (GTK_CONTAINER (clock_frame), clock_widget);

	/* The table layout container to pack the board and clocks */
	GtkWidget *left_grid;

	left_grid = gtk_grid_new();

	gtk_widget_set_hexpand(board_frame, TRUE);
	gtk_widget_set_vexpand(board_frame, TRUE);
	gtk_grid_attach(GTK_GRID(left_grid), board_frame,
	                0, // guint left_attach
	                1, // guint top attach
	                1, // guint width
	                clock_board_ratio + 1); // guint height
	gtk_widget_set_hexpand(clock_frame, TRUE);
	gtk_widget_set_vexpand(clock_frame, TRUE);
	gtk_grid_attach(GTK_GRID(left_grid), clock_frame,
	                0, // guint left attach
	                0, // guint top attach
	                1, // guint width
	                1); // guint height

	/* Channels Hashmap: we use this to quickly map a channel number to its index */
	channel_map = g_hash_table_new(g_int_hash, g_int_equal);
	reverse_channel_map = g_hash_table_new(g_direct_hash, g_direct_equal);

	/* Create empty notebook */
	channels_notebook = gtk_notebook_new();
	gtk_widget_set_size_request(channels_notebook, -1, 200);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(channels_notebook), TRUE);
	//gtk_notebook_popup_enable(GTK_NOTEBOOK(channels_notebook));

	GtkWidget *collapsible_analysis = gtk_expander_new_with_mnemonic("Computer _Analysis");
	gtk_container_add(GTK_CONTAINER(collapsible_analysis), create_analysis_panel());
	gtk_expander_set_expanded(GTK_EXPANDER(collapsible_analysis), true);

	// Pack analysis pane and moves list into a wrapper
	GtkWidget *analysis_wrapper = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_paned_pack1(GTK_PANED(analysis_wrapper), moves_v_box, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(analysis_wrapper), collapsible_analysis, FALSE, FALSE);

	// The right split pane
	GtkWidget *right_split_pane = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_paned_pack1(GTK_PANED(right_split_pane), analysis_wrapper, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(right_split_pane), channels_notebook, FALSE, FALSE);

	// The master horizontal split pane
	gtk_paned_pack1(GTK_PANED(split_pane), left_grid, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(split_pane), right_split_pane, FALSE, FALSE);

	gtk_paned_set_position(GTK_PANED(split_pane), (gint) (-16 + ((double) clock_board_ratio) / ((double) clock_board_ratio + 1.0f) * win_def_hi));

	gtk_container_add(GTK_CONTAINER (main_window), split_pane);

	gtk_window_set_title(GTK_WINDOW (main_window), "Cairo-Board");
	gtk_widget_set_name(GTK_WIDGET (main_window), "Mother of all windows");
	gtk_widget_set_name(GTK_WIDGET (board), "Board Area");

	needs_update = 0;
	needs_scale = 0;

	/* Connect signals */
	gtk_widget_add_events (board, GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK);

 	g_signal_connect (main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	g_signal_connect (board, "draw", G_CALLBACK(on_board_draw), NULL);

	g_signal_connect (G_OBJECT(board), "button-press-event", G_CALLBACK(on_button_press), NULL);
	g_signal_connect (G_OBJECT(board), "button-release-event", G_CALLBACK(on_button_release), NULL);
	g_signal_connect (G_OBJECT(board), "motion_notify_event", G_CALLBACK(on_motion), NULL);
	g_signal_connect (G_OBJECT(board), "configure_event", G_CALLBACK(on_configure_event), NULL);
	g_signal_connect (G_OBJECT(board), "got-move", G_CALLBACK(on_get_move), NULL);
	g_signal_connect (G_OBJECT(board), "got-crafty-move", G_CALLBACK(on_get_crafty_move), NULL);
	g_signal_connect (G_OBJECT(board), "got-uci-move", G_CALLBACK(on_get_uci_move), NULL);
	g_signal_connect (G_OBJECT(board), "flip-board", G_CALLBACK(on_flip_board), NULL);

	/* Get the pieces SVG dimensions for rendering */
	RsvgDimensionData g_DimensionData;
	rsvg_handle_get_dimensions (piecesSvg[B_QUEEN], &g_DimensionData);
	svg_w = 1.0f / (8.0f * (double) g_DimensionData.width);
	svg_h = 1.0f / (8.0f * (double) g_DimensionData.height);

	set_moveit_flag(false);
	set_running_flag(true);
	set_more_events_flag(false);

	reset_game(false);

	gtk_widget_show_all(main_window);

	// only show this when we have tabs to show
	gtk_widget_hide(channels_notebook);

	if (load_file_specified) {
		if (!open_file(file_to_load)) {
			auto_play_timer = g_timeout_add(auto_play_delay, auto_play_one_move, board);
		}
	}

	if (ics_mode) {
		init_ics();
	}

//	if (crafty_mode) {
//		spawn_crafty();
//		debug("Spawned Crafty\n");
//		if (pipe(crafty_data_pipe)) {
//			perror("Pipe creation failed");
//			return 1;
//		}
//	}

	bool brainfish = false;
	g_idle_add(spawn_uci_engine_idle, &brainfish);

	spawn_mover();

	///////////////////////
//	test_random_animation();
//	test_crazy_flip();
//	test_random_channel_insert();
//	test_random_title();
//	start_new_uci_game();
	///////////////////////


	FT_Error error = FT_Init_FreeType(&library);
	FT_Face sevenSegmentFTFace;
	if (error) {
		perror("Failed to init font\n");
		return 1;
	}

	char font_file[] = "DSEG7Classic-Bold.ttf";
	error = FT_New_Face(library, font_file, 0, &sevenSegmentFTFace);
	if (error == FT_Err_Unknown_File_Format) {
		perror("Font format unsupported");
	} else if (error) {
		perror("font file could not be opened or read, or it is broken");
	} else {
		FcBool ok = FcConfigAppFontAddFile(FcConfigGetCurrent(), (FcChar8 *)(font_file));
		if (!ok) {
			perror("Could not load font in FontConfig");
		} else {
			debug("Successfully loaded font in FontConfig! %s %s\n", sevenSegmentFTFace->family_name, sevenSegmentFTFace->style_name);
		}
	}

	// Start Gdk Main Loop
	gtk_main();

	gdk_threads_leave();

	free(main_clock);
	game_free(main_game);

	cleanup(NULL);


	return 0;
}



