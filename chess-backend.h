
#ifndef __CHESS_BACKEND_H__
#define __CHESS_BACKEND_H__

#include "cairo-board.h"

extern int castle_state[2][2];
extern int en_passant[8];
extern int whose_turn;
extern int fifty_move_counter;
uint64_t current_hash;

/* indices work as follow: [col][row][piece->type] */
uint64_t zobrist_keys_squares[8][8][12];
uint64_t zobrist_keys_en_passant[8];
uint64_t zobrist_keys_blacks_turn;
uint64_t zobrist_keys_castle[2][2];


int get_square_colour(int col, int row);
int get_possible_moves(chess_piece*, chess_square[8][8], int[64][2], int);
int is_piece_under_attack_raw(chess_piece* piece, chess_square sq[8][8]);
int is_king_checked(int, chess_square[8][8]);
int is_check_mate(int whose_turn, chess_square sq[8][8]);
int is_stale_mate(int whose_turn, chess_square sq[8][8]);
void count_alive_pieces_by_type(int alive[12], chess_piece w_set[16], chess_piece b_set[16]);
int is_material_draw(chess_piece w_set[16], chess_piece b_set[16]);
int is_move_legal(chess_piece *piece, int col, int row, int whose_turn, chess_square sq[8][8]);
chess_piece *get_king(int, chess_square[8][8]);
void copy_situation(chess_square[8][8], chess_square[8][8], chess_piece[32]);
int can_castle(int color, int side, chess_square sq[8][8]);
void raw_move(chess_square sq[8][8], chess_piece *piece, int col, int row, int update_hash);
int is_fifty_move_counter_expired(void);
void init_en_passant(void);
void reset_en_passant(void);
int is_move_en_passant(chess_piece *piece, int col, int row, chess_square sq[8][8]);

void init_zobrist_keys();
void toggle_piece(chess_piece *piece);
void persist_hash(void);
void init_hash(chess_square sq[8][8]);
int check_hash_triplet(void);
void init_zobrist_hash_history(void);

void generate_fen_no_enpassant(char fen_string[128], chess_square sq[8][8], int castle_state[2][2], int whose_turn);
void generate_fen(char fen_string[128], chess_square sq[8][8], int castle_state[2][2], int en_passant[8], int whose_turn);

#endif

