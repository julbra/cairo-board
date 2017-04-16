// cairo-board.h - Copyright Julien Bramary 2008-2009

#ifndef __CAIRO_BOARD_H__
#define __CAIRO_BOARD_H__

#include <ft2build.h>
#include FT_FREETYPE_H
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <wchar.h>

#include "clocks.h"

/* debug macro */
#ifndef debug
#ifdef colour_console
void debug(format, ...) { if (debug_flag) fprintf(stdout, "%s:\033[31m%d\033[0m " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define debug(format, ...) if (debug_flag) fprintf(stdout, "%s:%d " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

// Arg values for getopt
// NB: Do not use '?' (63) as it has a special meaning for getopt
#define ICS_HOST_ARG		2
#define ICS_PORT_ARG		3
#define LIGHT_RED_ARG		4
#define LIGHT_GREEN_ARG		5
#define LIGHT_BLUE_ARG		6
#define DARK_RED_ARG		7
#define DARK_GREEN_ARG		8
#define DARK_BLUE_ARG		9
#define LOAD_FILE_ARG		10
#define LOAD_GAME_NUM_ARG	11
#define AUTO_PLAY_DELAY_ARG	12
#define ICS_TEST_HANDLE1	13
#define ICS_TEST_HANDLE2	14
#define ICS_TEST_PLAYER1	15

// base unicode char for chess fonts
#define BASE_CHESS_UNICODE_CHAR 0x2654

#define WHITE false
#define BLACK true

#define SAN_MOVE_SIZE 16
#define MOVE_BUFF_SIZE 32

typedef struct {
    unsigned int row : 3; // range 0-7
    unsigned int column : 3; // range 0-7
} location;

typedef struct {
    unsigned int type : 4; // 0-12
    bool dead : 1; // 0-1
    bool colour : 1; // 0 -> white 1 -> black
    location pos;
    cairo_surface_t *surf;
} chess_piece;

typedef struct {
	int ply_number;
	unsigned int old_col : 3;
	unsigned int old_row : 3;
	unsigned int new_col : 3;
	unsigned int new_row : 3;
	unsigned int promo_type : 3;
	chess_piece *piece_taken;
	char san_string[16];
} ply;

typedef struct {
	chess_piece *piece;
} chess_square;

typedef struct {
	chess_piece white_set[16];
	chess_piece black_set[16];
	chess_square squares[8][8];

	unsigned int current_move_number;
	int promo_type;

	/* *
	 * castling state variables
	 * these are used for permanent prohibitions when a rook or king has moved
	 * it doesn't check for transient impossibilities due to checks etc...
	 * format is:
	 * castle_state[colour][side]
	 * colour: 0 -> white 1 -> black
	 * side:  0 -> left  1 -> right
	 * */
	int castle_state[2][2];
	int en_passant[8];
	int whose_turn;
	int fifty_move_counter;

	uint64_t current_hash;
	uint64_t zobrist_hash_history[50];
	int hash_history_index;

	char white_name[256];
	char black_name[256];
	char white_rating[32];
	char black_rating[32];

	char *moves_list; // String of the current moves list in SAN notation

	unsigned int ply_num;

} chess_game;

ply *ply_new(int oc, int or, int nc, int nr, chess_piece *taken, const char *san);

/* *
 * NB: most chess games contain less than 256 plys (128 moves).
 * We use dynamic (re)allocation to cope with longer games.
 * This avoids requiring a huge static allocation; which would be a 
 * waste and maybe not even enough:
 * 		Some clever people worked out that the longest theoretical 
 * 		chess game contains around 6k moves (12k plys).
 *
 * In practice, the longest played game contained 574 plys (237 moves).
 * Note: this game was possible because, at the time, FIDE had 
 * increased the 50-move allowance to 100
 * */
#define MOVES_LIST_ALLOC_PAGE_SIZE 256

/* NB: the list must be terminated by a NULL ply*/
typedef struct {
    ply **plys;
    int plys_allocated;
    int last_ply;
    int viewed_ply;
} plys_list;

plys_list *plys_list_new(void);
void plys_list_free(plys_list *to_destroy);
void plys_list_append_ply(plys_list *list, ply *to_append);
void plys_list_print(plys_list *list);

enum {
	KILLED_BY_NONE = 0,
	KILLED_BY_INSTANT_MOVE_TAKING,
	KILLED_BY_OTHER_ANIMATION_TAKING,
	KILLED_BY_OTHER_ANIMATION_SAME_PIECE,
};

struct anim_data {
	chess_piece *piece;
	double **plots;
	int n_plots;
	int step_index;
	int old_col;
	int old_row;
	int new_col;
	int new_row;
	int move_result;
	int promo_type;
	int move_source;
	int killed_by;
};

enum {
	W_KING = 0,
	W_QUEEN,
	W_ROOK,
	W_BISHOP,
	W_KNIGHT,
	W_PAWN,
	B_KING,
	B_QUEEN,
	B_ROOK,
	B_BISHOP,
	B_KNIGHT,
	B_PAWN
};

enum {
	PAWN1 = 0,
	PAWN2,
	PAWN3,
	PAWN4,
	PAWN5,
	PAWN6,
	PAWN7,
	PAWN8,
	ROOK1,
	KNIGHT1,
	BISHOP1,
	QUEEN,
	KING,
	BISHOP2,
	KNIGHT2,
	ROOK2
};

enum {
	// Move types
	REFUSED = -1,
	NORMAL,				// 0000
	CASTLE,				// 0001
	EN_PASSANT,			// 0010
	PROMOTE 	= 1 << 2,	// 0100
	PIECE_TAKEN	= 1 << 3,	// 1000
	MOVE_TYPE_MASK	= 15,		// 1111

	// Move details
	MOVE_DETAIL_MASK	= 15 << 4,	// 11110000

	// Castle details
	W_CASTLE_LEFT 		= 1 << 4,	// 00010000
	W_CASTLE_RIGHT 		= 2 << 4,	// 00100000
	B_CASTLE_LEFT 		= 4 << 4,	// 01000000
	B_CASTLE_RIGHT 		= 8 << 4,	// 10000000

	// Promotion details
	PROMOTE_QUEEN		= 1 << 4,	// 00010000
	PROMOTE_ROOK		= 2 << 4,	// 00010000
	PROMOTE_BISHOP		= 4 << 4,	// 00010000
	PROMOTE_KNIGHT		= 8 << 4,	// 00010000
};


enum {
	WHITE_WINS = 0,
	BLACK_WINS,
	DRAW,
	OTHER
};

enum {
	MANUAL_SOURCE = 0,
	AUTO_SOURCE,
	AUTO_SOURCE_NO_ANIM,
	PRE_MOVE
};

enum {
	MANUAL_PLAY = 0,
	I_PLAY_WHITE,
	I_PLAY_BLACK,
	I_OBSERVE,
};

extern bool clock_started;
extern bool game_started;
extern long my_game;

extern gboolean debug_flag;
extern gboolean use_fig;
extern gboolean ics_mode;
extern bool guest_mode;
extern bool load_file_specified;

extern char ics_host[256];
extern unsigned short ics_port;
extern char my_password[128];

extern double svg_w, svg_h;
extern double dr,dg, db;
extern double lr, lg, lb;
extern double highlight_selected_r, highlight_selected_g, highlight_selected_b, highlight_selected_a;
extern double highlight_move_r, highlight_move_g, highlight_move_b, highlight_move_a;
extern double highlight_pre_move_r, highlight_pre_move_g, highlight_pre_move_b, highlight_pre_move_a;
extern GdkRGBA chat_handle_colour;
extern bool invert_fig_colours;
extern double check_warn_r, check_warn_g, check_warn_b, check_warn_a;

extern int needs_update;
extern int needs_scale;

extern int mouse_clicked[2];
extern int old_wi, old_hi;
extern double w_ratio;
extern double h_ratio;

extern int game_mode;
extern bool play_vs_machine;
extern bool playing;

extern GtkWidget *main_window;
extern GtkWidget *board;

extern char last_san_move[SAN_MOVE_SIZE];
extern int resolved_move[4];
extern chess_piece *last_piece_taken;

extern bool delay_from_promotion;
extern int p_old_col, p_old_row;

extern plys_list *main_list;

void get_last_move_xy(int *x, int*y);
void get_last_release_xy(int *x, int*y);
void get_dragging_prev_xy(double *x, double*y);
void set_dragging_prev_xy(double, double);
bool is_moveit_flag();
void set_moveit_flag(bool);
bool is_running_flag();
bool is_more_events_flag();
void set_more_events_flag(bool);
void set_board_flipped(bool val);
bool is_board_flipped();
void get_pre_move(int premove[4]);
void set_pre_move(int premove[4]);
void unset_pre_move();

extern chess_piece *to_promote;

extern gboolean highlight_moves;
extern gboolean has_chosen;
extern bool highlight_last_move;

extern chess_clock *main_clock;
chess_game *main_game;

extern FILE *san_scanner_in;
extern char *ics_scanner_text;
extern char *san_scanner_text;

/* exported helpers */
int colorise_type(int tt, int colour);
void assign_surfaces();
void piece_to_xy(chess_piece *piece, double *xy ,int wi, int hi);
void loc_to_xy(int column, int row, double *xy, int wi, int hi);
int char_to_type(int whose_turn, char c);
char type_to_char(int);
char type_to_fen_char(int type);
int move_piece(chess_piece *piece, int col, int row, int check_legality, int move_source, char san_move[SAN_MOVE_SIZE], chess_game *game, bool logical_only);
void send_to_ics(char *s);
void send_to_uci(char *s);
void insert_san_move(const char*, bool should_lock_threads);
void check_ending_clause(chess_game *game);
void xy_to_loc(int x, int y, int *pos, int wi, int hi);
chess_square *xy_to_square(chess_game *game, int x, int y, int wi, int hi);
void flip_board(int wi, int hi);
wint_t type_to_unicode_char(int type);
bool can_i_move_piece(chess_piece* piece);
void set_last_move(char *move);
void start_game(char *w_name, char *b_name, int seconds, int increment, int relation, bool should_lock);
void end_game(void);
void update_eco_tag(bool should_lock_threads);
void popup_join_channel_dialog(bool lock_threads);
int resolve_move(chess_game *game, int t, char *move, int resolved_move[4]);
void add_class(GtkWidget *, const char *);
void insert_text_moves_list_view(const gchar *text, bool should_lock_threads);
void refresh_moves_list_view(plys_list *list);

void show_login_dialog(bool lock_threads);
void close_login_dialog(bool lock_threads);

/********** FROM DRAWING BACKEND *************/
extern RsvgHandle *piecesSvg[12];
gboolean auto_move(chess_piece *piece, int new_col, int new_row, int check_legality, int move_source, bool logical_only);

/* Flex macros */
#ifndef YY_TYPEDEF_YY_SIZE_T
#define YY_TYPEDEF_YY_SIZE_T
typedef unsigned int yy_size_t;
#endif

#ifndef YY_STRUCT_YY_BUFFER_STATE
#define YY_STRUCT_YY_BUFFER_STATE
struct yy_buffer_state
{
	FILE *yy_input_file;

	char *yy_ch_buf;		/* input buffer */
	char *yy_buf_pos;		/* current position in input buffer */

	/* Size of input buffer in bytes, not including room for EOB
	 * characters.
	 */
	int yy_buf_size;

	/* Number of characters read into yy_ch_buf, not including EOB
	 * characters.
	 */
	int yy_n_chars;

	/* Whether we "own" the buffer - i.e., we know we created it,
	 * and can realloc() it to grow it, and should free() it to
	 * delete it.
	 */
	int yy_is_our_buffer;

	/* Whether this is an "interactive" input source; if so, and
	 * if we're using stdio for input, then we want to use getc()
	 * instead of fread(), to make sure we stop fetching input after
	 * each newline.
	 */
	int yy_is_interactive;

	/* Whether we're considered to be at the beginning of a line.
	 * If so, '^' rules will be active on the next match, otherwise
	 * not.
	 */
	int yy_at_bol;

	int yy_bs_lineno; /**< The line count. */
	int yy_bs_column; /**< The column count. */

	/* Whether to try to fill the input buffer when we reach the
	 * end of it.
	 */
	int yy_fill_buffer;

	int yy_buffer_status;

#define YY_BUFFER_NEW 0
#define YY_BUFFER_NORMAL 1
	/* When an EOF's been seen but there's still some text to process
	 * then we mark the buffer as YY_EOF_PENDING, to indicate that we
	 * shouldn't try reading from the input source any more.  We might
	 * still have a bunch of tokens to match, though, because of
	 * possible backing-up.
	 *
	 * When we actually see the EOF, we change the status to "new"
	 * (via san_scanner_restart()), so that the user can continue scanning by
	 * just pointing san_scanner_in at a new input file.
	 */
#define YY_BUFFER_EOF_PENDING 2

};
#endif /* !YY_STRUCT_YY_BUFFER_STATE */

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

#endif

