#include <stdlib.h>
#include <malloc.h>

#include "chess-backend.h"
#include "cairo-board.h"

/* Returns the colour of the square[col][row]
 * 0 -> white
 * 1 -> black */
int get_square_colour(int col, int row) {

	int ret = 0;

	ret += col %2;
	ret += row %2;
	ret %= 2;

	return ret;
}

chess_piece *get_king(int colour, chess_square sq[8][8]) {

	int i,j;
	chess_piece *king = NULL;

	int match = (colour ? B_KING : W_KING);

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			if (sq[i][j].piece != NULL) {
				king = sq[i][j].piece;
				if (king->type == match ) {
					return king;
				}
			}
		}
	}
	// Can't happen
	return NULL;
}

void init_en_passant(chess_game *game) {
	int i;
	for (i = 0; i < 8; i++) {
		game->en_passant[i] = 0;
	}
}

void reset_en_passant(chess_game *game) {
	int i;
	for (i = 0; i < 8; i++) {
		if (game->en_passant[i]) {
			game->en_passant[i] = 0;
			game->current_hash ^= zobrist_keys_en_passant[i];
		}
	}
}


int is_fifty_move_counter_expired(chess_game *game) {
	return game->fifty_move_counter <= 0;
}

//chess_piece *find_piece(chess_piece set[16], int col, int row) {
//	for (int i = 0; i < 16; i++) {
//		if (set[i].pos.column == col && set[i].pos.row == row) {
//			return &(set[i]);
//		}
//	}
//	debug("clone_game NO found piece!\n");
//	return NULL;
//}

void clone_game(chess_game *src_game, chess_game *trans_game) {
	int i,j;

	// Sanity check positions
//	for (i = 0; i < 8; i++) {
//		for (j = 0; j < 8; j++) {
//			chess_piece *p = src_game->squares[i][j].piece;
//			if (p != NULL && (p->pos.column != i || p->pos.row != j)) {
//				debug("Aha!!!!!!!!!!!!!!!!!!!!\n");
//			}
//		}
//	}

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			trans_game->squares[i][j].piece = NULL;
		}
	}

	for (i = 0; i < 16; i++) {
		trans_game->black_set[i] = src_game->black_set[i];
		trans_game->white_set[i] = src_game->white_set[i];

		chess_piece *bp = &(trans_game->black_set[i]);
		if (!bp->dead) {
			trans_game->squares[bp->pos.column][bp->pos.row].piece = bp;
		}
		chess_piece *wp = &(trans_game->white_set[i]);
		if (!wp->dead) {
			trans_game->squares[wp->pos.column][wp->pos.row].piece = wp;
		}
	}

//	int w_count = 0;
//	int b_count = 0;
//	for (i = 0; i < 8; i++) {
//		for (j = 0; j < 8; j++) {
//			if (src_game->squares[i][j].piece != NULL) {
//				if (src_game->squares[i][j].piece->colour) {
////					trans_game->squares[i][j].piece = find_piece(trans_game->black_set, i, j);
//					trans_game->black_set[b_count] = *(src_game->squares[i][j].piece);
//					trans_game->squares[i][j].piece = &trans_game->black_set[b_count];
//					b_count++;
//				} else {
////					trans_game->squares[i][j].piece = find_piece(trans_game->white_set, i, j);
//					trans_game->white_set[w_count] = *(src_game->squares[i][j].piece);
//					trans_game->squares[i][j].piece = &trans_game->white_set[w_count];
//					w_count++;
//				}
//			} else {
//				trans_game->squares[i][j].piece = NULL;
//			}
//		}
//	}

//	for (i = 0; i < 8; i++) {
//		for (j = 0; j < 8; j++) {
//			chess_piece *p1 = trans_game->squares[i][j].piece;
//			if (p1 != NULL) {
//				chess_piece *p2 = find_piece((p1->colour ? trans_game->black_set : trans_game->white_set), i, j);
//				if (p1 != p2) {
//					debug("clone_game bug? %p %p\n", p1, p2);
//				}
//			}
//		}
//	}

	trans_game->whose_turn = src_game->whose_turn;
	trans_game->current_move_number = src_game->current_move_number;
	for (i = 0; i < 2; ++i) {
		for (j = 0; j < 2; ++j) {
			trans_game->castle_state[i][j] = src_game->castle_state[i][j];
		}
	}
	for (i = 0; i < 8; ++i) {
		trans_game->en_passant[i] = src_game->en_passant[i];
	}
	trans_game->fifty_move_counter = src_game->fifty_move_counter;
	trans_game->current_hash = src_game->current_hash;
	for (i = 0; i < 50; ++i) {
		trans_game->zobrist_hash_history[i] = src_game->zobrist_hash_history[i];
	}
	trans_game->hash_history_index = src_game->hash_history_index;
}

/* *
 * Castling has 3 caveats
 * 1. 	Neither king nor rook may have moved from its
 * 	original position (static check).
 * 2. 	It can only occur if there are no pieces standing
 * 	between the king and the rook (transient check).
 * 3. 	There can be no opposing piece that could possibly
 * 	capture the king in his original square, the square
 * 	he moves through nor the square that he ends up on
 * 	(transient check).
 * */
int can_castle(int colour, int side, chess_game *game) {

	// check for 1. (static check)
	if (!game->castle_state[colour][side]) {
		return 0;
	}

	// check for 2. (no pieces in between)
	if (!colour) { // white
		if (side) { // white right side
			if (game->squares[5][0].piece != NULL || game->squares[6][0].piece != NULL) {
				return 0;
			}
		} else { // white left side
			if (game->squares[1][0].piece != NULL || game->squares[2][0].piece != NULL || game->squares[3][0].piece != NULL) {
				return 0;
			}
		}
	} else { // black
		if (side) { // black right side
			if (game->squares[5][7].piece != NULL || game->squares[6][7].piece != NULL) {
				return 0;
			}
		} else { // black left side
			if (game->squares[1][7].piece != NULL || game->squares[2][7].piece != NULL || game->squares[3][7].piece != NULL) {
				return 0;
			}
		}
	}

	// check for 3 (most expensive check)
	if (is_king_checked(game, colour)) {
		return 0;
	}
	chess_game *trans_game = game_new();
	clone_game(game, trans_game);

	chess_piece *king = get_king(colour, trans_game->squares);

	// Do the proposed move on the transient set of pieces
	raw_move(trans_game, king, king->pos.column+(side?1:-1), king->pos.row, 0);
	if (is_king_checked(trans_game, colour)) {
		return 0;
	}
	raw_move(trans_game, king, king->pos.column+(side?1:-1), king->pos.row, 0);
	if (is_king_checked(trans_game, colour)) {
		return 0;
	}

	game_free(trans_game);

	// all conditions met
	return 1;
}

void raw_move(chess_game *game, chess_piece *piece, int col, int row, int update_hash) {
	int i, j;

	if (update_hash) {
		// remove piece at old position from hash
		toggle_piece(game, piece);
	}

	// get old position
	i = piece->pos.column;
	j = piece->pos.row;

	// clean out source square
	game->squares[i][j].piece = NULL;

	// set new position
	piece->pos.column = col;
	piece->pos.row = row;

	// Handle killed piece if any
	chess_piece *to_kill = game->squares[col][row].piece;
	if ( to_kill != NULL) {
		// removed killed piece from hash
		toggle_piece(game, to_kill);
		to_kill->dead = 1;
	}

	// instate square->piece link
	game->squares[col][row].piece = piece;

	if (update_hash) {
		// add piece at new position to hash
		toggle_piece(game, piece);
	}
}



int is_check_mate(chess_game *game) {
	if (!is_king_checked(game, game->whose_turn)) {
		return 0;
	}

	int count;
	int selected_moves[64][2];
	int i,j,k;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			chess_piece *piece = game->squares[i][j].piece;
			if (piece != NULL && piece->colour == game->whose_turn) {
				// don't consider castle: since the king is checked castling is illegal
				count = get_possible_moves(game, piece, selected_moves, 0);
				for (k = 0; k < count; k++) {
					if (is_move_legal(game, piece, selected_moves[k][0], selected_moves[k][1]) ) {
						//printf("No mate: found legal move %c%c%c\n", type_to_char(piece->type), 'a'+selected_moves[k][0], '1'+selected_moves[k][1]);
						return 0;
					}
				}
			}
		}
	}
	return 1;
}

int is_stale_mate(chess_game *game) {

	if (is_king_checked(game, game->whose_turn)) {
		return 0;
	}

	int count;
	int selected_moves[64][2];
	int i,j,k;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			chess_piece *piece = game->squares[i][j].piece;
			if (piece != NULL && piece->colour == game->whose_turn) {
				// don't consider castle
				// if king can castle he can do other things so no stalemate
				count = get_possible_moves(game, piece, selected_moves, 0);
				for (k = 0; k < count; k++) {
					if (is_move_legal(game, piece, selected_moves[k][0], selected_moves[k][1]) ) {
						return 0;
					}
				}
			}
		}
	}
	return 1;
}

void count_alive_pieces_by_type(int alive[12], chess_piece w_set[16], chess_piece b_set[16]) {
	int i;

	for (i = 0; i < 12; i++) {
		alive[i] = 0;
	}

	for (i = 0; i < 16; i++) {
		if ( ! w_set[i].dead) {
			alive[w_set[i].type]++;
		}
		if ( ! b_set[i].dead) {
			alive[b_set[i].type]++;
		}
	}
}

/* Situation where we might declare a material draw:
 * king versus king
 * king and bishop versus king
 * king and knight versus king
 * king and bishop versus king and bishop with the bishops
 * on the same colour. (Any number of additional bishops
 * of either colour on the same colour of square
 * due to underpromotion do not affect the situation.)
 */
int is_material_draw(chess_piece w_set[16], chess_piece b_set[16]) {
	int alive[12];
	count_alive_pieces_by_type(alive, w_set, b_set);

	// if a pawn is alive, no draw
	if (alive[W_PAWN] || alive[B_PAWN]) {
		return 0;
	}

	// if a rook is alive, no draw
	if (alive[W_ROOK] || alive[B_ROOK]) {
		return 0;
	}

	// if a queen is alive, no draw
	if (alive[W_QUEEN] || alive[B_QUEEN]) {
		return 0;
	}

	/* at this point we know there are
	 * no pawns, no queens nor rooks
	 * i.e. we're left with kings knights and bishops */

	// if knight and bishop of same player, no draw
	if (alive[W_KNIGHT] && alive[W_BISHOP]) {
		return 0;
	}
	if (alive[B_KNIGHT] && alive[B_BISHOP]) {
		return 0;
	}

	/* Neither side has both a knight and a bishop */

	// Check for 2 bishops of different square-colour
	int i;
	int alive_bishops[2] = {0, 0};
	if (alive[W_BISHOP] > 1) {
		for (i = 0; i < 16; i++) {
			chess_piece *p = &(w_set[i]);
			if ( p->type == W_BISHOP && !p->dead ) {
				alive_bishops[get_square_colour(p->pos.column, p->pos.row)]++;
			}
		}
		// we have at least 2 bishops, if both square-colour counts > 0,
		// they are of different colours, no draw
		if (alive_bishops[0] && alive_bishops[1]) {
			return 0;
		}
	}

	alive_bishops[0] = alive_bishops[1] = 0;
	if (alive[B_BISHOP] > 1) {
		for (i = 0; i < 16; i++) {
			chess_piece *p = &(b_set[i]);
			if ( p->type == B_BISHOP && !p->dead ) {
				alive_bishops[get_square_colour(p->pos.column, p->pos.row)]++;
			}
		}
		// we have at least 2 bishops, if both square colour counts > 0,
		// they are of different colours, no draw
		if (alive_bishops[0] && alive_bishops[1]) {
			return 0;
		}
	}

	/* Neither side has different coloured bishops */

	// check for 2 knights
	if (alive[W_KNIGHT] > 1 || alive[B_KNIGHT] > 1) {
		return 0;
	}

	/* Neither side has more than 1 knight */

	// check for alive bishop each side
	// NB: we already know that neither side has different coloured bishops
	if (alive[W_BISHOP] > 0 && alive[B_BISHOP] > 0) {
		alive_bishops[0] = alive_bishops[1] = 0;
		/* both sides have bishops */
		for (i = 0; i < 16; i++) {
			chess_piece *p;

			p = &(b_set[i]);
			if ( p->type == B_BISHOP && !p->dead ) {
				alive_bishops[get_square_colour(p->pos.column, p->pos.row)]++;
			}

			p = &(w_set[i]);
			if ( p->type == W_BISHOP && !p->dead ) {
				alive_bishops[get_square_colour(p->pos.column, p->pos.row)]++;
			}
		}
		// each player has at least 1 bishop, if both square
		// colour counts > 0, they are of different colours, no draw
		// NB: we already know that neither side has different coloured bishops
		if (alive_bishops[0] && alive_bishops[1]) {
			return 0;
		}
	}

	/* else, this is a draw
	 * Only cases left are:
	 *
 	 * king versus king
	 * king and bishop versus king
	 * king and knight versus king
	 * king and bishop versus king and bishop with the same-colour bishops */
	return 1;
}


bool is_king_checked(chess_game *game, int colour) {
	return is_piece_under_attack_raw(game, get_king(colour, game->squares));
}

/* Determine whether piece may be under attack in passed situation */
bool is_piece_under_attack_raw(chess_game *game, chess_piece* piece) {
	int i,j,k;
	int colour = piece->colour;
	int col = piece->pos.column;
	int row = piece->pos.row;

	chess_piece *cur_piece;
	int count;
	int possible_moves[64][2];

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			cur_piece = game->squares[i][j].piece;
			if (cur_piece != NULL) {
				// Only deal with pieces of the opposite colour
				if (cur_piece->colour != colour) {
					count = get_possible_moves(game, cur_piece, possible_moves, 0);
					for (k = 0; k < count; k++) {
						if (possible_moves[k][0] == col && possible_moves[k][1] == row) {
							return true;
						}
					}
				}
			}
		}

	}
	return false;
}


// move should have already be filtered by get_possible_moves
// so some of these checks are redundant
int is_move_en_passant(chess_game *game, chess_piece *piece, int col, int row) {
	if ( piece->type != W_PAWN && piece->type != B_PAWN ) {
		return 0;
	}
	if (!game->en_passant[col]) {
		return 0;
	}

	if (row == piece->pos.row + (game->whose_turn ? -1 : 1 ) && col != piece->pos.column && game->squares[col][row].piece == NULL ) {
		return EN_PASSANT;
	}
	return 0;
}

bool is_move_possible(chess_game *game, chess_piece *piece, int col, int row) {
	int selected[64][2];
	int count = get_possible_moves(game, piece, selected, 1);
	for (int i = 0; i < count; i++) {
		if (selected[i][0] == col && selected[i][1] == row) {
			return true;
		}
	}
	return false;
}

bool is_move_legal(chess_game *game, chess_piece *piece, int col, int row) {

	// Player can't move if not his turn
	if (piece->colour != game->whose_turn) {
		return false;
	}

	int start_col = piece->pos.column;
	int start_row = piece->pos.row;
	int colour = piece->colour;

	if (!is_move_possible(game, piece, col, row)) {
		// Move not even possible for that piece
		// Don't bother checking for legality
		return false;
	}

	/* The move is possible but might not be legal
	 * Check that the move doesn't result in the
	 * king being in check */

	// First make a transient copy of the pieces' state
	// We'll apply changes to this transient copy and do our checks

	chess_game *trans_game = game_new();
	clone_game(game, trans_game);

	// get equivalent of selected piece from transient squares
	chess_piece *trans_piece = trans_game->squares[start_col][start_row].piece;


	/* There is a very special case where our king is checked
	 * by a pawn which we propose to take en-passant.
	 * This is the only case where a piece which checks our king
	 * is taken and is not on the destination square of the 
	 * proposed move. We need to remove that pawn from the 
	 * transient squares now */
	if (is_move_en_passant(trans_game, trans_piece, col, row)) {
		chess_square *to_kill = &(trans_game->squares[col][row + (game->whose_turn ? 1 : -1)]);
		// kill pawn
		to_kill->piece->dead = 1;
		to_kill->piece = NULL;
	}

	// Do the proposed move on the transient set of pieces
	raw_move(trans_game, trans_piece, col, row, 0);

	// Check that the proposed move does not leave or put our king in check
	int would_check = is_king_checked(trans_game, colour);
	if (would_check) {
		// Move not possible as would put/leave our king in check
		//printf("Proposed move would check our king\n");
		return false;
	}

	game_free(trans_game);

	return true;
}

/* marks a square as selected for the current operation */
static void select_square(int selected[64][2], int *count, int col, int row) {
	selected[*count][0] = col;
	selected[*count][1] = row;
	(*count)++;
}

/* List all possible moves for the piece
 * NOTE: we don't check for the absolute legality yet */
int get_possible_moves(chess_game *game, chess_piece *piece, int selected[64][2], int consider_castling_moves) {

//	debug("getting possible moves\n");
	int i;

	int start_col = piece->pos.column;
	int start_row = piece->pos.row;
	int colour = piece->colour;

	int count = 0;
	switch (piece->type) {

		case W_PAWN:
			if (start_row == 4) {
				if (start_col > 0 && game->en_passant[start_col-1]) {
					select_square(selected, &count, start_col-1, start_row + 1);
				}
				if (start_col < 7 && game->en_passant[start_col+1]) {
					select_square(selected, &count, start_col+1, start_row + 1);
				}
			}
			if (start_row < 7) {
				if (game->squares[start_col][start_row+1].piece == NULL) {
					select_square(selected, &count, start_col, start_row + 1);
					if (start_row == 1)
						if (game->squares[start_col][start_row + 2].piece == NULL)
							select_square(selected, &count, start_col, start_row + 2);
				}
				if (start_col > 0)
				if (game->squares[start_col - 1][start_row + 1].piece != NULL && game->squares[start_col - 1][start_row + 1].piece->colour)
					select_square(selected, &count, start_col - 1, start_row + 1);
				if (start_col < 7)
				if (game->squares[start_col + 1][start_row + 1].piece != NULL && game->squares[start_col + 1][start_row + 1].piece->colour)
					select_square(selected, &count, start_col + 1, start_row + 1);
			}
		break;

		case B_PAWN:
			if (start_row == 3) {
				if (start_col > 0 && game->en_passant[start_col-1]) {
					select_square(selected, &count, start_col-1, start_row - 1);
				}
				if (start_col < 7 && game->en_passant[start_col+1]) {
					select_square(selected, &count, start_col+1, start_row - 1);
				}
			}
			if (start_row > 0) {
				if (game->squares[start_col][start_row - 1].piece == NULL) {
					select_square(selected, &count, start_col, start_row - 1);
					if (start_row == 6)
						if (game->squares[start_col][start_row - 2].piece == NULL)
							select_square(selected, &count, start_col, start_row - 2);
				}
				if (start_col > 0)
				if (game->squares[start_col - 1][start_row - 1].piece != NULL && ! game->squares[start_col - 1][start_row - 1].piece->colour)
					select_square(selected, &count, start_col - 1, start_row - 1);
				if (start_col < 7)
				if (game->squares[start_col + 1][start_row - 1].piece != NULL && ! game->squares[start_col + 1][start_row - 1].piece->colour)
					select_square(selected, &count, start_col + 1, start_row - 1);
			}
		break;

		case W_KNIGHT:
		case B_KNIGHT:
			if (start_row < 7) {
				if (start_col - 2 >= 0 ) {
					if (game->squares[start_col - 2][start_row + 1].piece == NULL) {
						select_square(selected, &count, start_col - 2, start_row + 1);
					}
					else if (colour ?
						! game->squares[start_col - 2][start_row + 1].piece->colour :
						     game->squares[start_col - 2][start_row + 1].piece->colour)
							select_square(selected, &count, start_col - 2, start_row + 1);
				}
				if (start_col + 2 <= 7 ) {
					if (game->squares[start_col + 2][start_row + 1].piece == NULL) {
						select_square(selected, &count, start_col + 2, start_row + 1);
					}
					else if (colour ?
						! game->squares[start_col + 2][start_row + 1].piece->colour :
						     game->squares[start_col + 2][start_row + 1].piece->colour)
							select_square(selected, &count, start_col + 2, start_row + 1);
				}

				if (start_row < 6) {
					if (start_col - 1 >= 0 ) {
						if (game->squares[start_col - 1][start_row + 2].piece == NULL) {
							select_square(selected, &count, start_col - 1, start_row + 2);
						}
					else if (colour ?
						! game->squares[start_col - 1][start_row + 2].piece->colour :
						     game->squares[start_col - 1][start_row + 2].piece->colour)
							select_square(selected, &count, start_col - 1, start_row + 2);
					}
					if (start_col + 1 <= 7 ) {
						if (game->squares[start_col + 1][start_row + 2].piece == NULL) {
							select_square(selected, &count, start_col + 1, start_row + 2);
						}
						else if (colour ?
							! game->squares[start_col + 1][start_row + 2].piece->colour :
							     game->squares[start_col + 1][start_row + 2].piece->colour)
								select_square(selected, &count, start_col + 1, start_row + 2);
					}
				}
			}
			if (start_row > 0) {
				if (start_col - 2 >= 0 ) {
					if (game->squares[start_col - 2][start_row - 1].piece == NULL) {
						select_square(selected, &count, start_col - 2, start_row - 1);
					}
					else if (colour ?
						! game->squares[start_col - 2][start_row - 1].piece->colour :
						     game->squares[start_col - 2][start_row - 1].piece->colour)
							select_square(selected, &count, start_col - 2, start_row - 1);
				}
				if (start_col + 2 <= 7 ) {
					if (game->squares[start_col + 2][start_row - 1].piece == NULL) {
						select_square(selected, &count, start_col + 2, start_row - 1);
					}
					else if (colour ?
						! game->squares[start_col + 2][start_row - 1].piece->colour :
						     game->squares[start_col + 2][start_row - 1].piece->colour)
							select_square(selected, &count, start_col + 2, start_row - 1);
				}

				if (start_row > 1) {
					if (start_col - 1 >= 0 ) {
						if (game->squares[start_col - 1][start_row - 2].piece == NULL) {
							select_square(selected, &count, start_col - 1, start_row - 2);
						}
						else if (colour ?
							! game->squares[start_col - 1][start_row - 2].piece->colour :
							     game->squares[start_col - 1][start_row - 2].piece->colour)
								select_square(selected, &count, start_col - 1, start_row - 2);
					}
					if (start_col + 1 <= 7 ) {
						if (game->squares[start_col + 1][start_row - 2].piece == NULL) {
							select_square(selected, &count, start_col + 1, start_row - 2);
						}
						else if (colour ?
							! game->squares[start_col + 1][start_row - 2].piece->colour :
							     game->squares[start_col + 1][start_row - 2].piece->colour)
								select_square(selected, &count, start_col + 1, start_row - 2);
					}
				}
			}

		break;

		case W_BISHOP:
		case B_BISHOP:
			for (i=1;;i++) {
				if (start_col + i > 7 || start_row + i > 7)
					break;
				if (game->squares[start_col + i][start_row + i].piece == NULL)
					select_square(selected, &count, start_col + i, start_row + i);
				else {
					if (colour ?
						! game->squares[start_col + i][start_row + i].piece->colour :
						game->squares[start_col + i][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0 || start_row + i > 7)
					break;
				if (game->squares[start_col - i][start_row + i].piece == NULL)
					select_square(selected, &count, start_col - i, start_row + i);
				else {
					if (colour ?
						! game->squares[start_col - i][start_row + i].piece->colour :
						game->squares[start_col - i][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0 || start_row - i < 0)
					break;
				if (game->squares[start_col - i][start_row - i].piece == NULL)
					select_square(selected, &count, start_col - i, start_row - i);
				else {
					if (colour ?
						! game->squares[start_col - i][start_row - i].piece->colour :
						game->squares[start_col - i][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col + i > 7 || start_row - i < 0)
					break;
				if (game->squares[start_col + i][start_row - i].piece == NULL)
					select_square(selected, &count, start_col + i, start_row - i);
				else {
					if (colour ?
						! game->squares[start_col + i][start_row - i].piece->colour :
						game->squares[start_col + i][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row - i);
					}
					break;
				}
			}

		break;

		case W_ROOK:
		case B_ROOK:
			for (i=1;;i++) {
				if (start_row + i > 7)
					break;
				if (game->squares[start_col][start_row + i].piece == NULL)
					select_square(selected, &count, start_col, start_row + i);
				else {
					if (colour ?
						! game->squares[start_col][start_row + i].piece->colour :
						game->squares[start_col][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0)
					break;
				if (game->squares[start_col - i][start_row].piece == NULL)
					select_square(selected, &count, start_col - i, start_row);
				else {
					if (colour ?
						! game->squares[start_col - i][start_row].piece->colour :
						game->squares[start_col - i][start_row].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_row - i < 0)
					break;
				if (game->squares[start_col][start_row - i].piece == NULL)
					select_square(selected, &count, start_col, start_row - i);
				else {
					if (colour ?
						! game->squares[start_col][start_row - i].piece->colour :
						game->squares[start_col][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col + i > 7)
					break;
				if (game->squares[start_col + i][start_row].piece == NULL)
					select_square(selected, &count, start_col + i, start_row);
				else {
					if (colour ?
						! game->squares[start_col + i][start_row].piece->colour :
						game->squares[start_col + i][start_row].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row);
					}
					break;
				}
			}

		break;

		case W_QUEEN:
		case B_QUEEN:
			for (i=1;;i++) {
				if (start_col + i > 7 || start_row + i > 7)
					break;
				if (game->squares[start_col + i][start_row + i].piece == NULL)
					select_square(selected, &count, start_col + i, start_row + i);
				else {
					if (colour ?
						! game->squares[start_col + i][start_row + i].piece->colour :
						game->squares[start_col + i][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0 || start_row + i > 7)
					break;
				if (game->squares[start_col - i][start_row + i].piece == NULL)
					select_square(selected, &count, start_col - i, start_row + i);
				else {
					if (colour ?
						! game->squares[start_col - i][start_row + i].piece->colour :
						game->squares[start_col - i][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0 || start_row - i < 0)
					break;
				if (game->squares[start_col - i][start_row - i].piece == NULL)
					select_square(selected, &count, start_col - i, start_row - i);
				else {
					if (colour ?
						! game->squares[start_col - i][start_row - i].piece->colour :
						game->squares[start_col - i][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col + i > 7 || start_row - i < 0)
					break;
				if (game->squares[start_col + i][start_row - i].piece == NULL)
					select_square(selected, &count, start_col + i, start_row - i);
				else {
					if (colour ?
						! game->squares[start_col + i][start_row - i].piece->colour :
						game->squares[start_col + i][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_row + i > 7)
					break;
				if (game->squares[start_col][start_row + i].piece == NULL)
					select_square(selected, &count, start_col, start_row + i);
				else {
					if (colour ?
						! game->squares[start_col][start_row + i].piece->colour :
						game->squares[start_col][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0)
					break;
				if (game->squares[start_col - i][start_row].piece == NULL)
					select_square(selected, &count, start_col - i, start_row);
				else {
					if (colour ?
						! game->squares[start_col - i][start_row].piece->colour :
						game->squares[start_col - i][start_row].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_row - i < 0)
					break;
				if (game->squares[start_col][start_row - i].piece == NULL)
					select_square(selected, &count, start_col, start_row - i);
				else {
					if (colour ?
						! game->squares[start_col][start_row - i].piece->colour :
						game->squares[start_col][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col + i > 7)
					break;
				if (game->squares[start_col + i][start_row].piece == NULL)
					select_square(selected, &count, start_col + i, start_row);
				else {
					if (colour ?
						! game->squares[start_col + i][start_row].piece->colour :
						game->squares[start_col + i][start_row].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row);
					}
					break;
				}
			}

		break;

		case W_KING:
		case B_KING:
			if (consider_castling_moves) {
				if (can_castle(colour, 0, game)) { // can castle left
					select_square(selected, &count, start_col - 2, start_row);
				}
				if (can_castle(colour, 1, game)) { // can castle right
					select_square(selected, &count, start_col + 2, start_row);
				}
			}
			if (start_col > 0) {
				if (game->squares[start_col - 1][start_row].piece == NULL)
					select_square(selected, &count, start_col - 1, start_row);
				else {
					if (colour ?
						! game->squares[start_col - 1][start_row].piece->colour :
						game->squares[start_col - 1][start_row].piece->colour)
					{
						select_square(selected, &count, start_col - 1, start_row);
					}
				}
			}
			if (start_col < 7) {
				if (game->squares[start_col + 1][start_row].piece == NULL)
					select_square(selected, &count, start_col + 1, start_row);
				else {
					if (colour ?
						! game->squares[start_col + 1][start_row].piece->colour :
						game->squares[start_col + 1][start_row].piece->colour)
					{
						select_square(selected, &count, start_col + 1, start_row);
					}
				}
			}
			if (start_row < 7) {
				if (game->squares[start_col][start_row + 1].piece == NULL)
					select_square(selected, &count, start_col, start_row + 1);
				else {
					if (colour ?
						! game->squares[start_col][start_row + 1].piece->colour :
						game->squares[start_col][start_row + 1].piece->colour)
					{
						select_square(selected, &count, start_col, start_row + 1);
					}
				}
				if (start_col > 0) {
					if (game->squares[start_col - 1][start_row + 1].piece == NULL)
						select_square(selected, &count, start_col - 1, start_row + 1);
					else {
						if (colour ?
							! game->squares[start_col - 1][start_row + 1].piece->colour :
							game->squares[start_col - 1][start_row + 1].piece->colour)
						{
							select_square(selected, &count, start_col - 1, start_row + 1);
						}
					}
				}
				if (start_col < 7) {
					if (game->squares[start_col + 1][start_row + 1].piece == NULL)
						select_square(selected, &count, start_col + 1, start_row + 1);
					else {
						if (colour ?
							! game->squares[start_col + 1][start_row + 1].piece->colour :
							game->squares[start_col + 1][start_row + 1].piece->colour)
						{
							select_square(selected, &count, start_col + 1, start_row + 1);
						}
					}
				}
			}
			if (start_row > 0) {
				if (game->squares[start_col][start_row - 1].piece == NULL)
					select_square(selected, &count, start_col, start_row - 1);
				else {
					if (colour ?
						! game->squares[start_col][start_row - 1].piece->colour :
						game->squares[start_col][start_row - 1].piece->colour)
					{
						select_square(selected, &count, start_col, start_row - 1);
					}
				}
				if (start_col > 0) {
					if (game->squares[start_col - 1][start_row - 1].piece == NULL)
						select_square(selected, &count, start_col - 1, start_row - 1);
					else {
						if (colour ?
							! game->squares[start_col - 1][start_row - 1].piece->colour :
							game->squares[start_col - 1][start_row - 1].piece->colour)
						{
							select_square(selected, &count, start_col - 1, start_row - 1);
						}
					}
				}
				if (start_col < 7) {
					if (game->squares[start_col + 1][start_row - 1].piece == NULL)
						select_square(selected, &count, start_col + 1, start_row - 1);
					else {
						if (colour ?
							! game->squares[start_col + 1][start_row - 1].piece->colour :
							game->squares[start_col + 1][start_row - 1].piece->colour)
						{
							select_square(selected, &count, start_col + 1, start_row - 1);
						}
					}
				}
			}
		break;

		default:
		/* can't happen */
		break;
	}

	return count;
}


// Hash related functions

// Generates a pseudo random 64bit integer from two 32bit ones
uint64_t get_random_64b() {
	uint64_t a, b, ul;

	a = (uint64_t) rand();
	b = (uint64_t) rand();
	ul = (a << 32) + b;

	return ul;
}

// Toggle piece to hash
void toggle_piece(chess_game *game, chess_piece *piece) {
	game->current_hash ^= zobrist_keys_squares[piece->pos.column][piece->pos.row][piece->type];
}

void init_zobrist_keys() {
	int i,j,k;

	// init seed
	srand(12345678);

	// Squares
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 12; k++) {
				zobrist_keys_squares[i][j][k] = get_random_64b();
			}
		}
	}

	// Castling rights
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			zobrist_keys_castle[i][j] = get_random_64b();
		}
	}

	// En-passant enabled columns
	for (i = 0; i < 8; i++) {
		zobrist_keys_en_passant[i] = get_random_64b();
	}

	// Black's turn
	zobrist_keys_blacks_turn = get_random_64b();
}

void init_zobrist_hash_history(chess_game *game) {
	int i;
	for (i = 0; i < 50; i++) {
		game->zobrist_hash_history[i] = 0;
	}
}

uint64_t generate_zobrist_hash(chess_game *game) {
	int i, j;

	uint64_t hash = 0;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			chess_piece *piece = game->squares[i][j].piece;
			if (piece != NULL) {
				hash ^= zobrist_keys_squares[i][j][piece->type];
			}
		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			if (game->castle_state[i][j]) {
				hash ^= zobrist_keys_castle[i][j];
			}
		}
	}

	for (i = 0; i < 8; i++) {
		if (game->en_passant[i]) {
			hash ^= zobrist_keys_en_passant[i];
		}
	}

	if (game->whose_turn) {
		hash ^= zobrist_keys_blacks_turn;
	}

	return hash;
}

// Recomputes the hash from scratch
void init_hash(chess_game *game) {
	game->current_hash = generate_zobrist_hash(game);
}

chess_game *game_new() {
	chess_game *new_game = malloc(sizeof(chess_game));
	if (!new_game) {
		perror("Malloc new_game failed");
		return NULL;
	}
	new_game->ply_num = 1;
	new_game->hash_history_index = 0;
	new_game->moves_list = calloc(256, SAN_MOVE_SIZE);
	return new_game;
}

void game_free(chess_game *game) {
	free(game->moves_list);
	free(game);
}

void append_san_move(chess_game *game, const char *san_move) {
	// Whose-turn has already been swapped
	char *append = calloc(SAN_MOVE_SIZE, sizeof(char));
	if (game->ply_num == 1) {
		sprintf(append, "1.%s", san_move);
	} else {
		if (game->whose_turn) {
			sprintf(append, " %d.%s", 1 + (game->ply_num / 2), san_move);
		} else {
			sprintf(append, " %s", san_move);
		}
	}
	game->ply_num++;

	size_t cur_len = strlen(game->moves_list);
	size_t append_len = strlen(append);
	size_t available = malloc_usable_size(game->moves_list);
	size_t required = cur_len + append_len;
	while (available < required) {
		game->moves_list = (char *) realloc(game->moves_list, malloc_usable_size(game->moves_list) * 2);
		available = malloc_usable_size(game->moves_list);
	}
	memcpy(game->moves_list + cur_len, append, append_len);
	free(append);
}

// Saves the current hash to history and increment the hash_index
void persist_hash(chess_game *game) {
	game->zobrist_hash_history[game->hash_history_index] = game->current_hash;
	game->hash_history_index++;
	game->hash_history_index %= 50;
}

int check_hash_triplet(chess_game *game) {
	int i;

	int match = 0;
	int start_index = game->hash_history_index - 1;
	if (start_index < 0) {
		start_index = 49;
	}
	int count = start_index;

	// compare last hash with history, starting from most recent
	// if 2 matches are found return 1
	for (i = 0; i < 49; i++) {
		count--;
		if (count < 0) {
			count = 49;
		}
		if (game->zobrist_hash_history[start_index] == game->zobrist_hash_history[count]) {
			match++;
		}
		if (match > 1) {
			return 1;
		}
	}
	return 0;
}

void generate_fen_no_enpassant(char fen_string[128], chess_square sq[8][8], int castle_state[2][2], int whose_turn) {

	int rank;
	int file;

	/* piece placement */
	int empty;
	int offset = 0;
	for (rank = 7; rank >= 0; rank--) {
		empty = 0;
		for (file = 0; file < 8; file++) {
			if (sq[file][rank].piece) {
				if (empty) {
					fen_string[offset++] = '0'+empty;
					empty = 0;
				}
				fen_string[offset++] = type_to_fen_char(sq[file][rank].piece->type);
			}
			else {
				empty++;
			}
		}
		if (empty) {
			fen_string[offset++] = '0'+empty;
		}
		fen_string[offset++] = '/';
	}
	fen_string[--offset] = ' ';
	offset++;

	/* active color*/
	fen_string[offset++] = (whose_turn?'b':'w');
	fen_string[offset++] = ' ';

	/*castling rights*/
	int is_castle = 0;
	if (castle_state[0][1]) {
		is_castle++;
		fen_string[offset++] = 'K';
	}
	if (castle_state[0][0]) {
		is_castle++;
		fen_string[offset++] = 'Q';
	}
	if (castle_state[1][1]) {
		is_castle++;
		fen_string[offset++] = 'k';
	}
	if (castle_state[1][1]) {
		is_castle++;
		fen_string[offset++] = 'q';
	}
	if (!is_castle) {
		fen_string[offset++] = '-';
	}
	fen_string[offset++] = ' ';

	fen_string[offset++] = '-';

	/* NULL terminate it */
	fen_string[offset++] = 0;
}

void generate_fen(char fen_string[128], chess_square sq[8][8], int castle_state[2][2], int en_passant[8], int whose_turn) {

	int rank;
	int file;

	// piece placement
	int empty;
	int offset = 0;
	for (rank = 7; rank >= 0; rank--) {
		empty = 0;
		for (file = 0; file < 8; file++) {
			if (sq[file][rank].piece) {
				if (empty) {
					fen_string[offset++] = (char) ('0' + empty);
					empty = 0;
				}
				fen_string[offset++] = type_to_fen_char(sq[file][rank].piece->type);
			}
			else {
				empty++;
			}
		}
		if (empty) {
			fen_string[offset++] = (char) ('0' + empty);
		}
		fen_string[offset++] = '/';
	}
	fen_string[--offset] = ' ';
	offset++;

	// active color
	fen_string[offset++] = (char) (whose_turn ? 'b' : 'w');
	fen_string[offset++] = ' ';

	// castling rights
	int is_castle = 0;
	if (castle_state[0][1]) {
		is_castle++;
		fen_string[offset++] = 'K';
	}
	if (castle_state[0][0]) {
		is_castle++;
		fen_string[offset++] = 'Q';
	}
	if (castle_state[1][1]) {
		is_castle++;
		fen_string[offset++] = 'k';
	}
	if (castle_state[1][1]) {
		is_castle++;
		fen_string[offset++] = 'q';
	}
	if (!is_castle) {
		fen_string[offset++] = '-';
	}
	fen_string[offset++] = ' ';

	// en-passant square
	int is_enpassant = 0;
	for (file = 0; file < 8; file++) {
		if (en_passant[file]) {
			is_enpassant++;
			fen_string[offset++] = (char) ('a' + file);
			fen_string[offset++] = (char) (whose_turn ? '3' : '6');
		}
	}
	if (!is_enpassant) {
		fen_string[offset++] = '-';
	}
	// NULL terminate it
	fen_string[offset] = 0;
}

void generate_full_fen(char fen_string[128], chess_square sq[8][8], int castle_state[2][2], int en_passant[8], int whose_turn, int fifty_move_counter, int full_move_number) {
	char temp_string[128];

	generate_fen(temp_string, sq, castle_state, en_passant, whose_turn);

	snprintf(fen_string, 128, "%s %d %d", temp_string, 99-fifty_move_counter, full_move_number);
}
