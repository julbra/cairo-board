
#ifndef __CHESS_BACKEND_H__
#define __CHESS_BACKEND_H__

#include "cairo-board.h"

static uint64_t zobrist_keys_squares[8][8][12];
static uint64_t zobrist_keys_en_passant[8];
static uint64_t zobrist_keys_blacks_turn;
static uint64_t zobrist_keys_castle[2][2];

chess_game *game_new();

void game_free(chess_game *game);

int get_square_colour(int col, int row);

void append_san_move(chess_game *game, const char *san_move);

int get_possible_moves(chess_game *game, chess_piece *, int[64][2], int);

int get_possible_pre_moves(chess_game *game, chess_piece *, int[64][2], int);

bool is_piece_under_attack_raw(chess_game *game, chess_piece *piece);

bool is_king_checked(chess_game *game, int colour);

int is_check_mate(chess_game *game);

int is_stale_mate(chess_game *game);

void count_alive_pieces_by_type(int alive[12], chess_piece w_set[16], chess_piece b_set[16]);

int is_material_draw(chess_piece w_set[16], chess_piece b_set[16]);

bool is_pre_move_possible(chess_game *game, chess_piece *piece, int col, int row);

bool is_move_possible(chess_game *game, chess_piece *piece, int col, int row);

bool is_move_legal(chess_game *game, chess_piece *piece, int col, int row);

chess_piece *get_king(int, chess_square[8][8]);

void clone_game(chess_game *src, chess_game *dst);

int can_castle(int color, int side, chess_game *game);

void raw_move(chess_game *game, chess_piece *piece, int col, int row, int update_hash);

int is_fifty_move_counter_expired(chess_game *game);

void init_en_passant(chess_game *game);

void reset_en_passant(chess_game *game);

int is_move_en_passant(chess_game *game, chess_piece *piece, int col, int row);

void init_zobrist_keys();

void toggle_piece(chess_game *game, chess_piece *piece);

void persist_hash(chess_game *game);

void init_hash(chess_game *game);

int check_hash_triplet(chess_game *game);

void init_zobrist_hash_history(chess_game *game);

void generate_fen_no_enpassant(char fen_string[128], chess_square sq[8][8], int castle_state[2][2], int whose_turn);

void generate_fen(char fen_string[128], chess_square sq[8][8], int castle_state[2][2], int en_passant[8], int whose_turn);

#endif

