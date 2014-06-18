#include <stdlib.h>

#include "chess-backend.h"

static uint64_t zobrist_hash_history[50];
static int hash_history_index = 0;

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

void init_en_passant(void) {
	int i;
	for (i = 0; i < 8; i++) {
		en_passant[i] = 0;
	}
}

void reset_en_passant(void) {
	int i;
	for (i = 0; i < 8; i++) {
		if (en_passant[i]) {
			en_passant[i] = 0;
			current_hash ^= zobrist_keys_en_passant[i];
		}
	}
}


int is_fifty_move_counter_expired(void) {
	return fifty_move_counter <= 0;
}

void copy_situation(chess_square source[8][8], chess_square dest[8][8], chess_piece copy[32]) {
	int i,j;
	int count = 0;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			if (source[i][j].piece != NULL) {
				copy[count] = *(source[i][j].piece);
				dest[i][j].piece = &copy[count];
				count++;
			}
			else {
				dest[i][j].piece = NULL;
			}

		}
	}
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
int can_castle(int colour, int side, chess_square sq[8][8]) {

	// check for 1. (static check)
	if ( ! castle_state[colour][side]) {
		return 0;
	}

	// check for 2. (no pieces in between)
	if ( ! colour ) { // white
		if ( side ) { // white right side
			if (sq[5][0].piece != NULL || sq[6][0].piece != NULL) {
				return 0;
			}
		}
		else { // white left side
			if (sq[1][0].piece != NULL || sq[2][0].piece != NULL || sq[3][0].piece != NULL) {
				return 0;
			}
		}
	}
	else { // black
		if ( side ) { // black right side
			if (sq[5][7].piece != NULL || sq[6][7].piece != NULL) {
				return 0;
			}
		}
		else { // black left side
			if (sq[1][7].piece != NULL || sq[2][7].piece != NULL || sq[3][7].piece != NULL) {
				return 0;
			}
		}
	}

	// check for 3 (most expensive check)
	if (is_king_checked(colour, sq)) {
		return 0;
	}
	chess_piece trans_set[32];
	chess_square trans_squares[8][8];
	copy_situation(sq, trans_squares, trans_set);

	chess_piece *king = get_king(colour, trans_squares);

	// Do the proposed move on the transient set of pieces
	raw_move(trans_squares, king, king->pos.column+(side?1:-1), king->pos.row, 0);
	if (is_king_checked(colour, trans_squares)) {
		return 0;
	}
	raw_move(trans_squares, king, king->pos.column+(side?1:-1), king->pos.row, 0);
	if (is_king_checked(colour, trans_squares)) {
		return 0;
	}

	// all conditions met
	return 1;
}

void raw_move(chess_square sq[8][8], chess_piece *piece, int col, int row, int update_hash) {
	int i, j;

	if (update_hash) {
		// remove piece at old position from hash
		toggle_piece(piece);
	}

	// get old position
	i = piece->pos.column;
	j = piece->pos.row;

	// clean out source square
	sq[i][j].piece = NULL;

	// set new position
	piece->pos.column = col;
	piece->pos.row = row;

	// Handle killed piece if any
	chess_piece *to_kill = sq[col][row].piece;
	if ( to_kill != NULL) {
		// removed killed piece from hash
		toggle_piece(to_kill);
		to_kill->dead = 1;
	}

	// instate square->piece link
	sq[col][row].piece = piece;

	if (update_hash) {
		// add piece at new position to hash
		toggle_piece(piece);
	}
}



int is_check_mate(int whose_turn, chess_square sq[8][8]) {
	if (!is_king_checked(whose_turn, sq)) {
		return 0;
	}

	int count;
	int selected_moves[64][2];
	int i,j,k;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			chess_piece *piece = sq[i][j].piece;
			if (piece != NULL && piece->colour == whose_turn) {
				// don't consider castle
				// if king can castle he can do other things so no checkmate
				count = get_possible_moves( piece, sq, selected_moves, 0);
				for (k = 0; k < count; k++) {
					if ( is_move_legal(piece, selected_moves[k][0], selected_moves[k][1], whose_turn, sq) ) {
						//printf("No mate: found legal move %c%c%c\n", type_to_char(piece->type), 'a'+selected_moves[k][0], '1'+selected_moves[k][1]);
						return 0;
					}
				}
			}
		}
	}
	return 1;
}

int is_stale_mate(int whose_turn, chess_square sq[8][8]) {

	if (is_king_checked(whose_turn, sq)) {
		return 0;
	}

	int count;
	int selected_moves[64][2];
	int i,j,k;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			chess_piece *piece = sq[i][j].piece;
			if (piece != NULL && piece->colour == whose_turn) {
				// don't consider castle
				// if king can castle he can do other things so no stalemate
				count = get_possible_moves( piece, sq, selected_moves, 0);
				for (k = 0; k < count; k++) {
					if ( is_move_legal(piece, selected_moves[k][0], selected_moves[k][1], whose_turn, sq) ) {
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


int is_king_checked(int colour, chess_square sq[8][8]) {
	return is_piece_under_attack_raw(get_king(colour, sq), sq);
}

/* Determine whether piece may be under attack in passed situation */
int is_piece_under_attack_raw(chess_piece* piece, chess_square sq[8][8]) {
	int i,j,k;
	int colour = piece->colour;
	int col = piece->pos.column;
	int row = piece->pos.row;

	chess_piece *cur_piece;
	int count;
	int possible_moves[64][2];

	int maybe_under_attack = 0;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			cur_piece = sq[i][j].piece;
			if (cur_piece != NULL) {
				// Only deal with pieces of the opposite colour
				if (cur_piece->colour != colour) {
					count = get_possible_moves(cur_piece, sq, possible_moves, 0);
					for (k = 0; k < count; k++) {
						if (possible_moves[k][0] == col && possible_moves[k][1] == row) {
							maybe_under_attack++;
							break;
						}
					}
				}
			}
		}

	}
	return maybe_under_attack;
}


// move should have already be filtered by get_possible_moves
// so some of these checks are redundant
int is_move_en_passant(chess_piece *piece, int col, int row, chess_square sq[8][8]) {
	if ( piece->type != W_PAWN && piece->type != B_PAWN ) {
		return 0;
	}
	if ( ! en_passant[col] ) {
		return 0;
	}

	if (row == piece->pos.row + (whose_turn ? -1 : 1 ) && col != piece->pos.column && sq[col][row].piece == NULL ) {
		return EN_PASSANT;
	}
	return 0;
}

int is_move_legal(chess_piece *piece, int col, int row, int blacks_ply, chess_square sq[8][8]) {

	// Player can't move if not his turn
	if (piece->colour != blacks_ply) {
		return 0;
	}

	int start_col = piece->pos.column;
	int start_row = piece->pos.row;
	int colour = piece->colour;
	int i;

	int selected[64][2];

	int count = get_possible_moves(piece, sq, selected, 1);

	int possible = 0;
	for (i = 0; i < count; i++) {
		if (selected[i][0] == col && selected[i][1] == row) {
			possible = 1;
			break;
		}
	}
	if (!possible) {
		// Move not even possible for that piece
		// Don't bother checking for legality
		return possible;
	}

	/* The move is possible but might not be legal
	 * Check that the move doesn't result in the
	 * king being in check */

	// First make a transient copy of the pieces' state
	// We'll apply changes to this transient copy and do our checks

	//chess_piece trans_white_set[16];
	//chess_piece trans_black_set[16];
	chess_piece trans_set[32];
	chess_square trans_squares[8][8];
	//copy_situation(white_set, black_set, trans_white_set, trans_black_set, trans_squares);
	copy_situation(sq, trans_squares, trans_set);

	// get equivalent of selected piece from transient squares
	chess_piece *trans_piece = trans_squares[start_col][start_row].piece;


	/* There is a very special case where our king is checked
	 * by a pawn which we propose to take en-passant.
	 * This is the only case where a piece which checks our king
	 * is taken and is not on the destination square of the 
	 * proposed move. We need to remove that pawn from the 
	 * transient squares now */
	if (is_move_en_passant(trans_piece, col, row, trans_squares)) {
		chess_square *to_kill = &(trans_squares[col][row + (blacks_ply ? 1 : -1) ]);
		// kill pawn
		to_kill->piece->dead = 1;
		to_kill->piece = NULL;
	}

	// Do the proposed move on the transient set of pieces
	raw_move(trans_squares, trans_piece, col, row, 0);

	// Check that the proposed move does not leave or put our king in check
	int would_check = is_king_checked(colour, trans_squares);
	if (would_check) {
		// Move not possible as would put/leave our king in check
		//printf("Proposed move would check our king\n");
		return 0;
	}

	return possible;
}

/* marks a square as selected for the current operation */
static void select_square(int selected[64][2], int *count, int col, int row) {
	selected[*count][0] = col;
	selected[*count][1] = row;
	(*count)++;
}

/* List all possible moves for the piece
 * NOTE: we don't check for the absolute legality yet */
int get_possible_moves(chess_piece *piece, chess_square sq[8][8], int selected[64][2], int consider_castling_moves) {

	int i;

	int start_col = piece->pos.column;
	int start_row = piece->pos.row;
	int colour = piece->colour;

	int count = 0;
	switch (piece->type) {

		case W_PAWN:
			if (start_row == 4) {
				if (start_col > 0 && en_passant[start_col-1]) {
					select_square(selected, &count, start_col-1, start_row + 1);
				}
				if (start_col < 7 && en_passant[start_col+1]) {
					select_square(selected, &count, start_col+1, start_row + 1);
				}
			}
			if (start_row < 7) {
				if (sq[start_col][start_row+1].piece == NULL) {
					select_square(selected, &count, start_col, start_row + 1);
					if (start_row == 1)
						if (sq[start_col][start_row + 2].piece == NULL)
							select_square(selected, &count, start_col, start_row + 2);
				}
				if (start_col > 0)
				if (sq[start_col - 1][start_row + 1].piece != NULL && sq[start_col - 1][start_row + 1].piece->colour)
					select_square(selected, &count, start_col - 1, start_row + 1);
				if (start_col < 7)
				if (sq[start_col + 1][start_row + 1].piece != NULL && sq[start_col + 1][start_row + 1].piece->colour)
					select_square(selected, &count, start_col + 1, start_row + 1);
			}
		break;

		case B_PAWN:
			if (start_row == 3) {
				if (start_col > 0 && en_passant[start_col-1]) {
					select_square(selected, &count, start_col-1, start_row - 1);
				}
				if (start_col < 7 && en_passant[start_col+1]) {
					select_square(selected, &count, start_col+1, start_row - 1);
				}
			}
			if (start_row > 0) {
				if (sq[start_col][start_row - 1].piece == NULL) {
					select_square(selected, &count, start_col, start_row - 1);
					if (start_row == 6)
						if (sq[start_col][start_row - 2].piece == NULL)
							select_square(selected, &count, start_col, start_row - 2);
				}
				if (start_col > 0)
				if (sq[start_col - 1][start_row - 1].piece != NULL && ! sq[start_col - 1][start_row - 1].piece->colour)
					select_square(selected, &count, start_col - 1, start_row - 1);
				if (start_col < 7)
				if (sq[start_col + 1][start_row - 1].piece != NULL && ! sq[start_col + 1][start_row - 1].piece->colour)
					select_square(selected, &count, start_col + 1, start_row - 1);
			}
		break;

		case W_KNIGHT:
		case B_KNIGHT:
			if (start_row < 7) {
				if (start_col - 2 >= 0 ) {
					if (sq[start_col - 2][start_row + 1].piece == NULL) {
						select_square(selected, &count, start_col - 2, start_row + 1);
					}
					else if (colour ?
						! sq[start_col - 2][start_row + 1].piece->colour :
						sq[start_col - 2][start_row + 1].piece->colour)
							select_square(selected, &count, start_col - 2, start_row + 1);
				}
				if (start_col + 2 <= 7 ) {
					if (sq[start_col + 2][start_row + 1].piece == NULL) {
						select_square(selected, &count, start_col + 2, start_row + 1);
					}
					else if (colour ?
						! sq[start_col + 2][start_row + 1].piece->colour :
						sq[start_col + 2][start_row + 1].piece->colour)
							select_square(selected, &count, start_col + 2, start_row + 1);
				}

				if (start_row < 6) {
					if (start_col - 1 >= 0 ) {
						if (sq[start_col - 1][start_row + 2].piece == NULL) {
							select_square(selected, &count, start_col - 1, start_row + 2);
						}
					else if (colour ?
						! sq[start_col - 1][start_row + 2].piece->colour :
						sq[start_col - 1][start_row + 2].piece->colour)
							select_square(selected, &count, start_col - 1, start_row + 2);
					}
					if (start_col + 1 <= 7 ) {
						if (sq[start_col + 1][start_row + 2].piece == NULL) {
							select_square(selected, &count, start_col + 1, start_row + 2);
						}
						else if (colour ?
							! sq[start_col + 1][start_row + 2].piece->colour :
							sq[start_col + 1][start_row + 2].piece->colour)
								select_square(selected, &count, start_col + 1, start_row + 2);
					}
				}
			}
			if (start_row > 0) {
				if (start_col - 2 >= 0 ) {
					if (sq[start_col - 2][start_row - 1].piece == NULL) {
						select_square(selected, &count, start_col - 2, start_row - 1);
					}
					else if (colour ?
						! sq[start_col - 2][start_row - 1].piece->colour :
						sq[start_col - 2][start_row - 1].piece->colour)
							select_square(selected, &count, start_col - 2, start_row - 1);
				}
				if (start_col + 2 <= 7 ) {
					if (sq[start_col + 2][start_row - 1].piece == NULL) {
						select_square(selected, &count, start_col + 2, start_row - 1);
					}
					else if (colour ?
						! sq[start_col + 2][start_row - 1].piece->colour :
						sq[start_col + 2][start_row - 1].piece->colour)
							select_square(selected, &count, start_col + 2, start_row - 1);
				}

				if (start_row > 1) {
					if (start_col - 1 >= 0 ) {
						if (sq[start_col - 1][start_row - 2].piece == NULL) {
							select_square(selected, &count, start_col - 1, start_row - 2);
						}
						else if (colour ?
							! sq[start_col - 1][start_row - 2].piece->colour :
							sq[start_col - 1][start_row - 2].piece->colour)
								select_square(selected, &count, start_col - 1, start_row - 2);
					}
					if (start_col + 1 <= 7 ) {
						if (sq[start_col + 1][start_row - 2].piece == NULL) {
							select_square(selected, &count, start_col + 1, start_row - 2);
						}
						else if (colour ?
							! sq[start_col + 1][start_row - 2].piece->colour :
							sq[start_col + 1][start_row - 2].piece->colour)
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
				if (sq[start_col + i][start_row + i].piece == NULL)
					select_square(selected, &count, start_col + i, start_row + i);
				else {
					if (colour ?
						! sq[start_col + i][start_row + i].piece->colour :
						sq[start_col + i][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0 || start_row + i > 7)
					break;
				if (sq[start_col - i][start_row + i].piece == NULL)
					select_square(selected, &count, start_col - i, start_row + i);
				else {
					if (colour ?
						! sq[start_col - i][start_row + i].piece->colour :
						sq[start_col - i][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0 || start_row - i < 0)
					break;
				if (sq[start_col - i][start_row - i].piece == NULL)
					select_square(selected, &count, start_col - i, start_row - i);
				else {
					if (colour ?
						! sq[start_col - i][start_row - i].piece->colour :
						sq[start_col - i][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col + i > 7 || start_row - i < 0)
					break;
				if (sq[start_col + i][start_row - i].piece == NULL)
					select_square(selected, &count, start_col + i, start_row - i);
				else {
					if (colour ?
						! sq[start_col + i][start_row - i].piece->colour :
						sq[start_col + i][start_row - i].piece->colour)
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
				if (sq[start_col][start_row + i].piece == NULL)
					select_square(selected, &count, start_col, start_row + i);
				else {
					if (colour ?
						! sq[start_col][start_row + i].piece->colour :
						sq[start_col][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0)
					break;
				if (sq[start_col - i][start_row].piece == NULL)
					select_square(selected, &count, start_col - i, start_row);
				else {
					if (colour ?
						! sq[start_col - i][start_row].piece->colour :
						sq[start_col - i][start_row].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_row - i < 0)
					break;
				if (sq[start_col][start_row - i].piece == NULL)
					select_square(selected, &count, start_col, start_row - i);
				else {
					if (colour ?
						! sq[start_col][start_row - i].piece->colour :
						sq[start_col][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col + i > 7)
					break;
				if (sq[start_col + i][start_row].piece == NULL)
					select_square(selected, &count, start_col + i, start_row);
				else {
					if (colour ?
						! sq[start_col + i][start_row].piece->colour :
						sq[start_col + i][start_row].piece->colour)
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
				if (sq[start_col + i][start_row + i].piece == NULL)
					select_square(selected, &count, start_col + i, start_row + i);
				else {
					if (colour ?
						! sq[start_col + i][start_row + i].piece->colour :
						sq[start_col + i][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0 || start_row + i > 7)
					break;
				if (sq[start_col - i][start_row + i].piece == NULL)
					select_square(selected, &count, start_col - i, start_row + i);
				else {
					if (colour ?
						! sq[start_col - i][start_row + i].piece->colour :
						sq[start_col - i][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0 || start_row - i < 0)
					break;
				if (sq[start_col - i][start_row - i].piece == NULL)
					select_square(selected, &count, start_col - i, start_row - i);
				else {
					if (colour ?
						! sq[start_col - i][start_row - i].piece->colour :
						sq[start_col - i][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col + i > 7 || start_row - i < 0)
					break;
				if (sq[start_col + i][start_row - i].piece == NULL)
					select_square(selected, &count, start_col + i, start_row - i);
				else {
					if (colour ?
						! sq[start_col + i][start_row - i].piece->colour :
						sq[start_col + i][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col + i, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_row + i > 7)
					break;
				if (sq[start_col][start_row + i].piece == NULL)
					select_square(selected, &count, start_col, start_row + i);
				else {
					if (colour ?
						! sq[start_col][start_row + i].piece->colour :
						sq[start_col][start_row + i].piece->colour)
					{
						select_square(selected, &count, start_col, start_row + i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col - i < 0)
					break;
				if (sq[start_col - i][start_row].piece == NULL)
					select_square(selected, &count, start_col - i, start_row);
				else {
					if (colour ?
						! sq[start_col - i][start_row].piece->colour :
						sq[start_col - i][start_row].piece->colour)
					{
						select_square(selected, &count, start_col - i, start_row);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_row - i < 0)
					break;
				if (sq[start_col][start_row - i].piece == NULL)
					select_square(selected, &count, start_col, start_row - i);
				else {
					if (colour ?
						! sq[start_col][start_row - i].piece->colour :
						sq[start_col][start_row - i].piece->colour)
					{
						select_square(selected, &count, start_col, start_row - i);
					}
					break;
				}
			}
			for (i=1;;i++) {
				if (start_col + i > 7)
					break;
				if (sq[start_col + i][start_row].piece == NULL)
					select_square(selected, &count, start_col + i, start_row);
				else {
					if (colour ?
						! sq[start_col + i][start_row].piece->colour :
						sq[start_col + i][start_row].piece->colour)
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
				if ( can_castle(colour, 0, sq) ) { // can castle left
					select_square(selected, &count, start_col - 2, start_row);
				}
				if ( can_castle(colour, 1, sq) ) { // can castle right
					select_square(selected, &count, start_col + 2, start_row);
				}
			}
			if (start_col > 0) {
				if (sq[start_col - 1][start_row].piece == NULL)
					select_square(selected, &count, start_col - 1, start_row);
				else {
					if (colour ?
						! sq[start_col - 1][start_row].piece->colour :
						sq[start_col - 1][start_row].piece->colour)
					{
						select_square(selected, &count, start_col - 1, start_row);
					}
				}
			}
			if (start_col < 7) {
				if (sq[start_col + 1][start_row].piece == NULL)
					select_square(selected, &count, start_col + 1, start_row);
				else {
					if (colour ?
						! sq[start_col + 1][start_row].piece->colour :
						sq[start_col + 1][start_row].piece->colour)
					{
						select_square(selected, &count, start_col + 1, start_row);
					}
				}
			}
			if (start_row < 7) {
				if (sq[start_col][start_row + 1].piece == NULL)
					select_square(selected, &count, start_col, start_row + 1);
				else {
					if (colour ?
						! sq[start_col][start_row + 1].piece->colour :
						sq[start_col][start_row + 1].piece->colour)
					{
						select_square(selected, &count, start_col, start_row + 1);
					}
				}
				if (start_col > 0) {
					if (sq[start_col - 1][start_row + 1].piece == NULL)
						select_square(selected, &count, start_col - 1, start_row + 1);
					else {
						if (colour ?
							! sq[start_col - 1][start_row + 1].piece->colour :
							sq[start_col - 1][start_row + 1].piece->colour)
						{
							select_square(selected, &count, start_col - 1, start_row + 1);
						}
					}
				}
				if (start_col < 7) {
					if (sq[start_col + 1][start_row + 1].piece == NULL)
						select_square(selected, &count, start_col + 1, start_row + 1);
					else {
						if (colour ?
							! sq[start_col + 1][start_row + 1].piece->colour :
							sq[start_col + 1][start_row + 1].piece->colour)
						{
							select_square(selected, &count, start_col + 1, start_row + 1);
						}
					}
				}
			}
			if (start_row > 0) {
				if (sq[start_col][start_row - 1].piece == NULL)
					select_square(selected, &count, start_col, start_row - 1);
				else {
					if (colour ?
						! sq[start_col][start_row - 1].piece->colour :
						sq[start_col][start_row - 1].piece->colour)
					{
						select_square(selected, &count, start_col, start_row - 1);
					}
				}
				if (start_col > 0) {
					if (sq[start_col - 1][start_row - 1].piece == NULL)
						select_square(selected, &count, start_col - 1, start_row - 1);
					else {
						if (colour ?
							! sq[start_col - 1][start_row - 1].piece->colour :
							sq[start_col - 1][start_row - 1].piece->colour)
						{
							select_square(selected, &count, start_col - 1, start_row - 1);
						}
					}
				}
				if (start_col < 7) {
					if (sq[start_col + 1][start_row - 1].piece == NULL)
						select_square(selected, &count, start_col + 1, start_row - 1);
					else {
						if (colour ?
							! sq[start_col + 1][start_row - 1].piece->colour :
							sq[start_col + 1][start_row - 1].piece->colour)
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
void toggle_piece(chess_piece *piece) {
	current_hash ^= zobrist_keys_squares[piece->pos.column][piece->pos.row][piece->type];
}

void init_zobrist_keys(void) {
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

void init_zobrist_hash_history(void) {
	int i;
	for (i = 0; i < 50; i++) {
		zobrist_hash_history[i] = 0;
	}
}

uint64_t generate_zobrist_hash(chess_square sq[8][8]) {
	int i, j;

	uint64_t hash = 0;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			chess_piece *piece = sq[i][j].piece;
			if (piece != NULL) {
				hash ^= zobrist_keys_squares[i][j][piece->type];
			}
		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			if (castle_state[i][j]) {
				hash ^= zobrist_keys_castle[i][j];
			}
		}
	}

	for (i = 0; i < 8; i++) {
		if (en_passant[i]) {
			hash ^= zobrist_keys_en_passant[i];
		}
	}

	if (whose_turn) {
		hash ^= zobrist_keys_blacks_turn;
	}

	return hash;
}

// Recomputes the hash from scratch
void init_hash(chess_square sq[8][8]) {
	current_hash = generate_zobrist_hash(sq);
}


// Saves the current hash to history and increment the hash_index
void persist_hash(void) {
	zobrist_hash_history[hash_history_index] = current_hash;
	hash_history_index++;
	hash_history_index %= 50;
}

int check_hash_triplet(void) {
	int i;

	int match = 0;
	int start_index = hash_history_index - 1;
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
		if (zobrist_hash_history[start_index] == zobrist_hash_history[count]) {
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

	/* en-passant square */
	int is_enpassant = 0;
	for (file = 0; file < 8; file++) {
		if (en_passant[file]) {
			is_enpassant++;
			fen_string[offset++] = 'a'+file;
			fen_string[offset++] = (whose_turn?'3':'6');
		}
	}
	if (!is_enpassant) {
		fen_string[offset++] = '-';
	}
	/* NULL terminate it */
	fen_string[offset++] = 0;
}

void generate_full_fen(char fen_string[128], chess_square sq[8][8], int castle_state[2][2], int en_passant[8], int whose_turn, int fifty_move_counter, int full_move_number) {
	char temp_string[128];

	generate_fen(temp_string, sq, castle_state, en_passant, whose_turn);

	snprintf(fen_string, 128, "%s %d %d", temp_string, 99-fifty_move_counter, full_move_number);
}
