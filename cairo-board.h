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

/* base unicode char for chess fonts */
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

typedef struct {
    chess_piece *piece;
} chess_square;

enum {
	KILLED_BY_NONE = 0,
	KILLED_BY_INSTANT_MOVE_TAKING,
	KILLED_BY_OTHER_ANIMATION_TAKING,
	KILLED_BY_OTHER_ANIMATION_SAME_PIECE,
};

struct anim_data {
	chess_piece *piece;
	int **plots;
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
	AUTO_SOURCE_NO_ANIM
};

enum {
	MANUAL_PLAY = 0,
	I_PLAY_WHITE,
	I_PLAY_BLACK,
	I_OBSERVE,
};

extern gboolean debug_flag;
extern gboolean use_fig;
extern gboolean crafty_mode;

extern double svg_w, svg_h;
extern double dr,dg, db;
extern double lr, lg, lb;

extern int needs_update;
extern int needs_scale;

extern chess_piece white_set[16];
extern chess_piece black_set[16];
extern chess_square squares[8][8];
extern gboolean flipped;

extern int mouse_clicked[2];
extern int old_wi, old_hi;
extern double w_ratio;
extern double h_ratio;

extern chess_piece *mouse_dragged_piece;
extern chess_piece *mouse_clicked_piece;

extern int game_mode;

extern int moveit_flag;
extern int running_flag;
extern int more_events_flag;

extern GtkWidget *board;

extern int dragging_prev_x;
extern int dragging_prev_y;

extern char last_san_move[SAN_MOVE_SIZE];
extern chess_piece *last_piece_taken;

extern int delay_from_promotion;
extern int p_old_col, p_old_row;

extern plys_list *main_list;
extern int promo_type;

extern gint last_move_x, last_move_y;
extern gint last_release_x, last_release_y;

extern chess_piece *to_promote;
extern gulong hide_handler_id;

extern gboolean highlight_moves;
extern gboolean has_chosen;
extern gboolean highlight_last_move;

extern chess_clock *main_clock;

extern cairo_font_face_t *sevenSegmentFace;


/* exported helpers */
void assign_surfaces(void);
void piece_to_xy(chess_piece *piece, int *xy ,int wi, int hi);
void loc_to_xy(int column, int row, int *xy, int wi, int hi);
int char_to_type(char c);
char type_to_char(int);
char type_to_fen_char(int type);
int move_piece(chess_piece *piece, int col, int row, int check_legality, int move_source, char san_move[SAN_MOVE_SIZE], int blacks_ply, chess_piece w_set[16], chess_piece b_set[16], gboolean lock_threads);
void send_to_ics(char *s);
void send_to_uci(char *s);
void insert_san_move(const char*, gboolean should_lock_threads);
void check_ending_clause(void);
void xy_to_loc(int x, int y, int *pos, int wi, int hi);
chess_square *xy_to_square(int x, int y, int wi, int hi);
void flip_board(int wi, int hi);
wint_t type_to_unicode_char(int type);
gboolean can_i_move_piece(chess_piece* piece);
void set_last_move(char *move);
void start_game(char *w_name, char *b_name, int seconds, int increment, int relation, bool should_lock);
void update_eco_tag(gboolean should_lock_threads);
void popup_join_channel_dialog(gboolean lock_threads);

/********** FROM DRAWING BACKEND *************/
extern RsvgHandle *piecesSvg[12];
gboolean auto_move(chess_piece *piece, int new_col, int new_row, int check_legality, int move_source);

#endif

