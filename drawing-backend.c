/*
 * drawing-backend.c
 *
 *  Created on: 24 Nov 2009
 *      Author: hts
 */
#include <stdlib.h>
#include <gtk/gtk.h>

#include <librsvg/rsvg.h>

#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "drawing-backend.h"
#include "cairo-board.h"
#include "chess-backend.h"
#include "crafty-adapter.h"

/* Prototypes */
static void clean_last_drag_step(cairo_t *cdc, double wi, double hi);
static void plot_coords(int start[2], int mid[2], int end[2], int points_to_plot, int **plots, int *nPlots);
static void free_anim_data(struct anim_data *anim);
static void highlight_square(cairo_t *dc, int col, int row, double r, double g, double b, double a, int wi, int hi);
static void update_dragging_background(chess_piece *piece, int wi, int hi);
static void restore_dragging_background(chess_piece *piece, int move_result, int wi, int hi);
static void logical_promote(int last_promote);

enum layer_id {
	LAYER_0 = 0,
	LAYER_1,
	LAYER_2,
	CACHE_LAYER,
	DRAGGING_BACKGROUND
};

cairo_surface_t *layer_0 = NULL;
cairo_surface_t *layer_1 = NULL;
cairo_surface_t *layer_2 = NULL;
cairo_surface_t *cache_layer = NULL;
cairo_surface_t *dragging_background = NULL;

cairo_surface_t *ds_surf = NULL;
cairo_surface_t *ls_surf = NULL;
RsvgHandle *piecesSvg[12];
cairo_surface_t *piecesSurf[12];

GHashTable *anims_map;

/* Show last move variables */
int lm_source_col = -1, lm_source_row;
int lm_dest_col, lm_dest_row;

void init_anims_map(void) {
	anims_map = g_hash_table_new(g_direct_hash, g_direct_equal);
}

struct anim_data *get_anim_for_piece(chess_piece *piece) {
	return g_hash_table_lookup(anims_map, piece);
}

void update_pieces_surfaces(int wi, int hi) {
	cairo_t* dc;
	int i;
	for (i = 0 ; i < 12 ; i++) {
		cairo_surface_destroy (piecesSurf[i]);
		piecesSurf[i] = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ((double)wi)/8.0f, ((double)hi)/8.0f);
		dc = cairo_create (piecesSurf[i]);
		cairo_scale(dc, wi*svg_w, hi*svg_h);
		rsvg_handle_render_cairo (piecesSvg[i], dc);
		cairo_destroy(dc);
	}
	assign_surfaces();
}

/* Internal convenience method */
void apply_surface_at(cairo_t *cdc, cairo_surface_t *surf, double x, double y, double w, double h) {
	cairo_set_source_surface(cdc, surf, x, y);
	cairo_rectangle(cdc, x, y, w, h);
	cairo_fill(cdc);
}

void draw_board_surface(int width, int height) {

	// Create a "memory-buffer" surface to draw on
	cairo_surface_destroy(layer_0);
	layer_0 = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(layer_0);

//	cairo_set_source_rgb(cr, .0, .0, .0);
//	cairo_paint(cr);

	int j,k;

	double tx = width/8.0;
	double ty = height/8.0;

//	cairo_pattern_t *dPattern = cairo_pattern_create_radial (tx/2.0, ty/2.0, .0, tx/2.0, ty/2.0, (tx+ty)/4.0f);
//	cairo_pattern_t *lPattern = cairo_pattern_create_radial (tx/2.0, ty/2.0, .0, tx/2.0, ty/2.0, (tx+ty)/4.0f);
//	cairo_pattern_add_color_stop_rgba (dPattern, 0.0f, 1, 1, 1, 0.1f);
//	cairo_pattern_add_color_stop_rgba (dPattern, 1.0f, 1, 1, 1, 0.0f);
//	cairo_pattern_add_color_stop_rgba (dPattern, 0.0f, 1, 1, 1, 0.4f);
//	cairo_pattern_add_color_stop_rgba (dPattern, 1.0f, 1, 1, 1, 0.0f);

	cairo_pattern_t *dPattern = cairo_pattern_create_radial(.0, .0, .0, .0, .0, (tx + ty) / 3.0f);
	cairo_pattern_t *lPattern = cairo_pattern_create_radial(.0, .0, .0, .0, .0, (tx + ty) / 3.0f);
	cairo_pattern_add_color_stop_rgba(lPattern, 0.0f, 0, 0, 0, 0.1f);
	cairo_pattern_add_color_stop_rgba(lPattern, 1.0f, 0, 0, 0, 0.0f);
	cairo_pattern_add_color_stop_rgba(dPattern, 0.0f, 1, 1, 1, 0.1f);
	cairo_pattern_add_color_stop_rgba(dPattern, 1.0f, 1, 1, 1, 0.0f);

//	cairo_pattern_t *dPattern = cairo_pattern_create_radial (.0, .0, .0, .0, .0, (tx+ty)/2.0f);
//	cairo_pattern_t *lPattern = cairo_pattern_create_radial (.0, .0, .0, .0, .0, (tx+ty)/2.0f);
//	cairo_pattern_add_color_stop_rgba (lPattern, 0.0f, 0, 0, 0, 0.4f);
//	cairo_pattern_add_color_stop_rgba (lPattern, 1.0f, 1, 0, 0, 0.0f);
//	cairo_pattern_add_color_stop_rgba (dPattern, 0.0f, 1, 1, 1, 0.4f);
//	cairo_pattern_add_color_stop_rgba (dPattern, 1.0f, 1, 1, 1, 0.0f);

	// Draw Dark Squares
	cairo_set_source_rgb(cr, dr, dg, db);
	for (j = 0; j < 8; j++) {
		for (k = 0; k < 8; k++) {
			if (get_square_colour(j, k)) {
				cairo_rectangle(cr, j*tx, k*ty, tx, ty);
				cairo_fill(cr);

                // Inner shadow effect
				cairo_save(cr);
				cairo_translate(cr, j*tx, k*ty);
				cairo_set_source (cr, dPattern);
				cairo_rectangle(cr, 0, 0, tx, ty);
				cairo_fill(cr);
				cairo_restore(cr);

			}
		}
	}

	// Draw Light Squares
	cairo_set_source_rgb (cr, lr, lg, lb);
	for (j = 0; j < 8; j++) {
		for (k = 0; k < 8; k++) {
			if (!get_square_colour(j, k)) {
				cairo_rectangle(cr, j*tx, k*ty, tx, ty);
				cairo_fill(cr);

				cairo_save(cr);
				cairo_translate(cr, j*tx, k*ty);
				cairo_set_source (cr, lPattern);
				cairo_rectangle(cr, 0, 0, tx, ty);
				cairo_fill(cr);
				cairo_restore(cr);
			}
		}
	}

	// Draw Grid
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgb(cr, (dr + lr) / 2.0f, (dg + lg) / 2.0f, (db + lb) / 2.0f);
	cairo_set_line_width(cr, 1.0f);

	// Vertical lines
	for (j = 0; j <= 8; j++) {
		cairo_move_to (cr, j * tx, 0);
		cairo_line_to(cr, j * tx, height);
	}
	// Horizontal lines
	for (j = 0; j <= 8; j++) {
		cairo_move_to (cr, 0, j * ty);
		cairo_line_to(cr, width, j * ty);
	}
	cairo_stroke (cr);
	cairo_destroy(cr);

}


void rebuild_surfaces(int swi, int shi) {
	// re-render source surfaces only if size has changed
	if (needs_update) {
		draw_board_surface(swi, shi);
		update_pieces_surfaces(swi, shi);
		needs_update = 0;
	}
}

void draw_pieces_surface(int width, int height) {

	cairo_t* dc = NULL;
	int i;

	cairo_surface_destroy(layer_1);
	layer_1 = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	dc = cairo_create(layer_1);

	int xy[2];
	for (i=0; i<16; i++) {
		if (! white_set[i].dead ) {
			piece_to_xy(&white_set[i], xy, width, height);
			apply_surface_at(dc, white_set[i].surf, xy[0]-width/16.0f , xy[1]-height/16.0f, width/8.0f, height/8.0f);
		}

		if (! black_set[i].dead ) {
			piece_to_xy(&black_set[i], xy, width, height);
			apply_surface_at(dc, black_set[i].surf, xy[0]-width/16.0f , xy[1]-height/16.0f, width/8.0f, height/8.0f);
		}

	}

	cairo_destroy(dc);

}

void update_pieces_surface_by_loc(int width, int height, int old_col, int old_row, int new_col, int new_row) {

	cairo_t* dc;
	int xy[2];

	// size of a square
	double ww = width/8.0f;
	double hh = height/8.0f;

	dc = cairo_create(layer_1);
	cairo_save(dc);

	// Clean out old piece from surface
	loc_to_xy(old_col, old_row, xy, width, height);
	cairo_rectangle(dc, floor(xy[0]-width/16.0f), floor(xy[1]-height/16.0f), ceil(ww), ceil(hh));
	cairo_clip(dc);
	cairo_set_source_rgba(dc, 0.0f, 0.0f, 0.0f, 0.0f);
	cairo_set_operator(dc, CAIRO_OPERATOR_SOURCE);
	cairo_paint(dc);

	cairo_restore(dc);

	// Add new piece to surface
	chess_piece *piece = NULL;
	if (new_col >= 0 && new_row >= 0) {
		piece = squares[new_col][new_row].piece;
	}
	if (piece != NULL) {
		loc_to_xy(piece->pos.column, piece->pos.row, xy, width, height);
		cairo_rectangle(dc, floor(xy[0]-width/16.0f), floor(xy[1]-height/16.0f), ceil(ww), ceil(hh));
		cairo_clip(dc);
		cairo_set_operator(dc, CAIRO_OPERATOR_SOURCE);
		apply_surface_at(dc, piece->surf, xy[0]-width/16.0f, xy[1]-height/16.0f, width/8.0f, height/8.0f);
	}
	cairo_destroy(dc);

}

// This must be called after a real move
void update_pieces_surface(int width, int height, int old_col, int old_row, chess_piece *piece) {
	cairo_t* dc;
	int xy[2];

	// size of a square
	double ww = width/8.0f;
	double hh = height/8.0f;

	dc = cairo_create(layer_1);
	cairo_save(dc);

	// Clean out old piece from surface
	loc_to_xy(old_col, old_row, xy, width, height);
	cairo_rectangle(dc, floor(xy[0]-width/16.0f), floor(xy[1]-height/16.0f), ceil(ww), ceil(hh));
	cairo_clip(dc);
	cairo_set_source_rgba(dc, 0.0f, 0.0f, 0.0f, 0.0f);
	cairo_set_operator(dc, CAIRO_OPERATOR_SOURCE);
	cairo_paint(dc);

	cairo_restore(dc);

	// Add new piece to surface
	if (piece != NULL) {
		loc_to_xy(piece->pos.column, piece->pos.row, xy, width, height);
		cairo_rectangle(dc, floor(xy[0]-width/16.0f), floor(xy[1]-height/16.0f), ceil(ww), ceil(hh));
		cairo_clip(dc);
		cairo_set_operator(dc, CAIRO_OPERATOR_SOURCE);
		apply_surface_at(dc, piece->surf, xy[0]-width/16.0f, xy[1]-height/16.0f, width/8.0f, height/8.0f);
	}
	cairo_destroy(dc);
}

void kill_piece_from_surface(int width, int height, int col, int row) {
	update_pieces_surface(width, height, col, row, NULL);
}

// Restore piece to layer_0 surface
void restore_piece_to_surface(int width, int height, chess_piece *piece) {

	// size of a square
	double ww = width/8.0f;
	double hh = height/8.0f;

	// Add new piece to surface
	if (piece != NULL) {
		int xy[2];
		cairo_t* dc = cairo_create(layer_1);
		loc_to_xy(piece->pos.column, piece->pos.row, xy, width, height);
		cairo_rectangle(dc, floor(xy[0]-width/16.0f), floor(xy[1]-height/16.0f), ceil(ww), ceil(hh));
		cairo_clip(dc);
		cairo_set_operator(dc, CAIRO_OPERATOR_SOURCE);
		apply_surface_at(dc, piece->surf, xy[0]-width/16.0f, xy[1]-height/16.0f, ww, hh);
		cairo_destroy(dc);
	}

}

void paint_layers(cairo_t *cdc) {

	cairo_set_operator (cdc, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface (cdc, layer_0, 0.0f, 0.0f);
	cairo_paint(cdc);

	cairo_set_operator (cdc, CAIRO_OPERATOR_OVER);
	cairo_set_source_surface (cdc, layer_1, 0.0f, 0.0f);
	cairo_paint(cdc);

	cairo_set_source_surface (cdc, layer_2, 0.0f, 0.0f);
	cairo_paint(cdc);

}

void init_highlight_surface(int wi, int hi) {
	cairo_surface_destroy(layer_2);
	layer_2 = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, wi, hi);
}

void expose_update(cairo_t *cdr, double wi, double hi) {

	rebuild_surfaces(wi, hi);
	draw_pieces_surface(wi, hi);

	// Re-highlight highlighted square if any
	init_highlight_surface(wi, hi);
	if (mouse_clicked[0] >= 0) {
		//debug("Rebuilding highlight %d,%d\n", mouse_clicked[0], mouse_clicked[1]);
		cairo_t *high_cr = cairo_create(layer_2);
		highlight_square(high_cr, mouse_clicked[0], mouse_clicked[1], 1, 1, 0, 1, wi, hi);
		cairo_destroy(high_cr);
	}

	if (highlight_last_move) {
		if (lm_source_col > -1) {
			// paint to highlight layer
			cairo_t *high_cr = cairo_create(layer_2);
			highlight_square(high_cr, lm_source_col, lm_source_row, 0, 255, 0, 1, wi, hi);
			highlight_square(high_cr, lm_dest_col, lm_dest_row, 0, 255, 0, 1, wi, hi);
			cairo_destroy(high_cr);
		}
	}

	// FIXME: is this the best place for this?
	init_dragging_background(wi, hi);
	old_wi = wi;
	old_hi = hi;

	cairo_surface_destroy(cache_layer);
	cache_layer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, wi, hi);
	cairo_t *cache_cr = cairo_create(cache_layer);
	paint_layers(cache_cr);
	if (mouse_dragged_piece != NULL && g_atomic_int_get(&moveit_flag)) {
		debug("Dragged while resetting!\n");
		int dragged_x = g_atomic_int_get(&dragging_prev_x);
		int dragged_y = g_atomic_int_get(&dragging_prev_y);
		cairo_set_source_surface (cache_cr, mouse_dragged_piece->surf, dragged_x-wi/16.0f, dragged_y-hi/16.0f);
		cairo_set_operator(cache_cr, CAIRO_OPERATOR_OVER);
		cairo_paint(cache_cr);
	}
	cairo_destroy(cache_cr);

	cairo_set_source_surface(cdr, cache_layer, 0, 0);
	cairo_set_operator(cdr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cdr);
}

void expose_scale(cairo_t *cdr, double wi, double hi) {
	// This is the whole point of having the cache layer:
	// For some reason it is *A LOT* quicker to rescale
	// the cache layer than to rescale layer0 and layer1 and then stack them
	w_ratio = ((double)wi) / ((double)old_wi);
	h_ratio = ((double)hi) / ((double)old_hi);
	cairo_scale(cdr, w_ratio, h_ratio);

	cairo_set_source_surface(cdr, cache_layer, 0, 0);
	cairo_set_operator(cdr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cdr);

	needs_scale = 0;
}

void expose_clip(cairo_t *cdr, GdkEventExpose* event, double wi, double hi) {

	// Clip if no need update
	cairo_t *cache_cr = cairo_create(cache_layer);
	cairo_rectangle (cache_cr, event->area.x, event->area.y, event->area.width, event->area.height);
	cairo_clip(cache_cr);
	paint_layers(cache_cr);
	if (mouse_dragged_piece != NULL && g_atomic_int_get(&moveit_flag)) {
		debug("Dragged while resetting!\n");
		int dragged_x = g_atomic_int_get(&dragging_prev_x);
		int dragged_y = g_atomic_int_get(&dragging_prev_y);
		cairo_set_source_surface (cache_cr, mouse_dragged_piece->surf, dragged_x-wi/16.0f, dragged_y-hi/16.0f);
		cairo_set_operator(cache_cr, CAIRO_OPERATOR_OVER);
		cairo_paint(cache_cr);
	}
	cairo_destroy(cache_cr);

	cairo_rectangle (cdr, event->area.x, event->area.y, event->area.width, event->area.height);
	cairo_clip(cdr);
	cairo_set_source_surface(cdr, cache_layer, 0, 0);
	cairo_set_operator(cdr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cdr);

}

/* convenience method, internal only */
static void preSelect(int on_off, cairo_t* dc, int i, int j, int wi, int hi) {
	int xy[2];
	double ww = wi/8.0f;
	//double hh = hi/8.0f;
	loc_to_xy(i, j, xy, wi, hi);

	cairo_arc(dc, xy[0], xy[1], on_off ? ww/2.2f : ww/2.2f + 1, 0, 2.0f*M_PI);
	cairo_close_path(dc);

	/*cairo_arc (cr, xy[0], xy[1], on_off ? ww/2.2f : ww/2.2f + 1, 0, 2.0f*M_PI);
	cairo_close_path(cr);*/

	//cairo_rectangle(dc, xy[0]-wi/16.0f + 2, xy[1]-hi/16.0f + 2, ww - 4, hh - 4);
}

/*
 * Paint to the main and rebuild the highlight_surface
 * (for later use) to show potential moves
 * This is broken for now, low priority
 */
void highlightPotentialMoves(GtkWidget *pWidget, chess_piece *piece, int wi, int hi, int on_off) {

	int i;

	if (whose_turn != piece->colour) {
		return;
	}

	cairo_t *bdc;
	cairo_t *ddc;

	cairo_t *cdr = gdk_cairo_create (pWidget->window);

	bdc = cairo_create(layer_2);
	ddc = cairo_create(dragging_background);

	int highlighted[64][2];
	int count;
	count = get_possible_moves(piece, squares, highlighted, 1);

	for (i = 0; i < count; i++) {
		preSelect(on_off, bdc, highlighted[i][0], highlighted[i][1], wi, hi);
		preSelect(on_off, ddc, highlighted[i][0], highlighted[i][1], wi, hi);
		preSelect(on_off, cdr, highlighted[i][0], highlighted[i][1], wi, hi);
	}

	if (on_off) { // do highlight
		cairo_set_source_rgba (bdc, 0.0f, 1.0f, 0.0f, .75f);
		cairo_set_operator(bdc, CAIRO_OPERATOR_SOURCE);
		cairo_fill (bdc);

		cairo_clip(cdr);
		cairo_clip(ddc);
		cairo_set_source_surface(cdr, layer_2, 0, 0);
		cairo_set_source_surface(ddc, layer_2, 0, 0);
		cairo_paint(cdr);
		cairo_paint(ddc);

	}
	else { // remove highlight
		cairo_set_source_rgba (bdc, 0, 0, 0, 0);
		cairo_set_operator (bdc, CAIRO_OPERATOR_SOURCE);
		cairo_fill(bdc);

		cairo_clip(cdr);
		cairo_clip(ddc);
		paint_layers(ddc);
		cairo_set_source_surface(cdr, cache_layer, 0, 0);
		cairo_paint(cdr);
	}
	cairo_destroy(bdc);
	cairo_destroy(cdr);

}

void de_highlight_square(cairo_t *dc, int col, int row, int wi, int hi) {

	double half_line_width = ((double)wi)/(8.0f*45.0f);
	if (half_line_width < 1.0f) {
		half_line_width = 1.0f;
	}
	double line_width = 4*half_line_width;

	int xy[2];
	loc_to_xy(col, row, xy, wi, hi);
	cairo_rectangle(dc, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(wi/8.0f+1), ceil(hi/8.0f+1));
	cairo_rectangle(dc, ceil(xy[0]-wi/16.0f+line_width+1), ceil(xy[1]-hi/16.0f+line_width+1), floor(wi/8.0f-2*line_width-2), floor(hi/8.0f-2*line_width-2));
	cairo_set_source_surface(dc, layer_0, 0, 0);
	cairo_set_fill_rule(dc, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_fill_preserve(dc);

	cairo_set_source_surface(dc, layer_1, 0, 0);
	cairo_fill(dc);
//	cairo_fill_preserve(dc);

	// debug
//	cairo_set_source_rgba(dc, 1, 0, 0, .3);
//	cairo_fill(dc);

}

void clean_highlight_surface(int col, int row, int wi, int hi) {

	int xy[2];
	loc_to_xy(col, row, xy, wi, hi);
	cairo_t *highlight = cairo_create(layer_2);
	cairo_rectangle(highlight, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(wi/8.0f+1), ceil(hi/8.0f+1));
	cairo_set_source_rgba(highlight, 0, 0, 0, 0);
	cairo_set_operator(highlight, CAIRO_OPERATOR_SOURCE);
	cairo_clip(highlight);
	cairo_paint(highlight);
	cairo_destroy(highlight);
}

void apply_highlighted_square_over(cairo_t *dc, int col, int row, int wi, int hi) {
	double half_line_width = ((double)wi)/(8.0f*45.0f);
	if (half_line_width < 1.0f) {
		half_line_width = 1.0f;
	}
	double line_width = 4*half_line_width;
	int xy[2];
	loc_to_xy(col, row, xy, wi, hi);

	cairo_save(dc);
	cairo_rectangle(dc, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(wi/8.0f+1), ceil(hi/8.0f+1));
	cairo_rectangle(dc, ceil(xy[0]-wi/16.0f+line_width+1), ceil(xy[1]-hi/16.0f+line_width+1), floor(wi/8.0f-2*line_width-2), floor(hi/8.0f-2*line_width-2));
	cairo_clip(dc);
	cairo_set_source_surface(dc, layer_2, 0, 0);
	cairo_paint(dc);
	cairo_restore(dc);
}


static gboolean animate_one_step(gpointer data) {

	if (!g_atomic_int_get(&running_flag)) {
		return FALSE;
	}

	int xx, yy;
	int prev_x, prev_y;

	struct anim_data *anim = (struct anim_data *)data;

	double wi = (double)board->allocation.width;
	double hi = (double)board->allocation.height;

	xx = anim->plots[anim->step_index][0];
	yy = anim->plots[anim->step_index][1];
	prev_x = anim->plots[anim->step_index-1][0];
	prev_y = anim->plots[anim->step_index-1][1];

	double ww = wi/8.0f;
	double hh = hi/8.0f;

	// First step, remove piece surface from layer_1
	if (anim->step_index < 2) {
		kill_piece_from_surface(wi, hi, anim->old_col, anim->old_row);
	}

	// Animation was killed, find out why
	if (anim->killed_by || anim->piece->dead) {
		if (anim->piece->dead) {
			debug("Piece was killed while being animated!\n");
		}


		gdk_threads_enter();
		{
			// handle promote
			if (anim->move_result > 0 && anim->move_result & PROMOTE && anim->move_source == AUTO_SOURCE) {
				debug("Promote from killed anim\n");
				to_promote = anim->piece;
				delay_from_promotion = FALSE;
				choose_promote(anim->promo_type, TRUE, anim->old_col, anim->old_row, anim->new_col, anim->new_row, FALSE);
			}

			// clean last step from dragging background
			cairo_t *dragging_dc = cairo_create(dragging_background);
			cairo_save(dragging_dc);
			cairo_rectangle(dragging_dc, floor(prev_x-wi/16), floor(prev_y-hi/16), ceil(ww), ceil(hh));
			cairo_clip(dragging_dc);
			paint_layers(dragging_dc);


			// paint buffer surface with dragging background
			cairo_t *cache_dc = cairo_create(cache_layer);
			cairo_save(cache_dc);
			cairo_set_source_surface (cache_dc, dragging_background, 0.0f, 0.0f);
			cairo_rectangle(cache_dc, floor(prev_x-wi/16), floor(prev_y-hi/16), ceil(ww), ceil(hh));
			cairo_clip(cache_dc);
			cairo_paint(cache_dc);



			cairo_t *cdr = gdk_cairo_create (board->window);
			cairo_rectangle(cdr, floor(prev_x-wi/16), floor(prev_y-hi/16), ceil(ww), ceil(hh));

			// In case anim was killed by other animated piece taking it or
			// new anim, repaint piece on its would have been destination
			if (anim->killed_by == KILLED_BY_OTHER_ANIMATION_TAKING ||
					anim->killed_by == KILLED_BY_OTHER_ANIMATION_SAME_PIECE) {
				debug("Animation was %s!\n",(anim->killed_by == KILLED_BY_OTHER_ANIMATION_TAKING?"KILLED_BY_OTHER_ANIMATION_TAKING":"KILLED_BY_OTHER_ANIMATION_SAME_PIECE"));
				int killed_xy[2];
				cairo_restore(dragging_dc);
				loc_to_xy(anim->new_col, anim->new_row, killed_xy, wi, hi);
				cairo_rectangle(dragging_dc, floor(killed_xy[0]-wi/16), floor(killed_xy[1]-hi/16), ceil(ww), ceil(hh));
				cairo_clip(dragging_dc);
				cairo_set_source_surface (dragging_dc, layer_0, 0.0f, 0.0f);
				cairo_paint(dragging_dc);
				cairo_set_source_surface(dragging_dc, anim->piece->surf, killed_xy[0]-wi/16, killed_xy[1]-hi/16);
				cairo_paint(dragging_dc);

				// debug
//				cairo_set_source_rgba (dragging_dc, 1, 0, 0, .5f);
//				cairo_paint(dragging_dc);

				cairo_restore(cache_dc);
				cairo_save(cache_dc);
				cairo_set_source_surface(cache_dc, dragging_background, 0, 0);
				cairo_rectangle(cache_dc, floor(killed_xy[0]-wi/16), floor(killed_xy[1]-hi/16), ceil(ww), ceil(hh));
				cairo_clip(cache_dc);
				cairo_paint(cache_dc);
				cairo_restore(cache_dc);
				cairo_rectangle(cdr, floor(killed_xy[0]-wi/16), floor(killed_xy[1]-hi/16), ceil(ww), ceil(hh));
			}

			// If a piece is being dragged and overlaps with the animation, repaint the dragged piece above to cache layer
			if (mouse_dragged_piece != NULL && g_atomic_int_get(&moveit_flag)) {
				int dragged_x = g_atomic_int_get(&dragging_prev_x);
				int dragged_y = g_atomic_int_get(&dragging_prev_y);
				//FIXME: better lock access to mouse_dragged_piece (this could segfault otherwise)
				cairo_set_source_surface (cache_dc, mouse_dragged_piece->surf, dragged_x-wi/16.0f, dragged_y-hi/16.0f);
				cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
				cairo_paint(cache_dc);
			}

			cairo_destroy(dragging_dc);
			cairo_destroy(cache_dc);

			cairo_clip(cdr);

			// apply buffered surface to cr (NB: cr is clipped)
			cairo_set_operator (cdr, CAIRO_OPERATOR_OVER);
			cairo_set_source_surface (cdr, cache_layer, 0.0f, 0.0f);
			cairo_paint(cdr);

			// debug
//			cairo_set_source_rgba (cdr, 1, 0, 0, .5f);
//			cairo_paint(cdr);

			cairo_destroy(cdr);
		}
		/* LEAVE THREADS */
		gdk_threads_leave();

		if (anim->killed_by != KILLED_BY_OTHER_ANIMATION_SAME_PIECE) {
			g_hash_table_remove(anims_map, anim->piece);
		}
		free_anim_data(anim);
		return FALSE;
	}

	/* ENTER THREADS */
	gdk_threads_enter();

	// clean last step from dragging background - [added since we now paint to draggin_layer]
	cairo_t *dragging_dc = cairo_create(dragging_background);
	cairo_save(dragging_dc);
	cairo_rectangle(dragging_dc, floor(prev_x-wi/16), floor(prev_y-hi/16), ceil(ww), ceil(hh));
	cairo_clip(dragging_dc);
	paint_layers(dragging_dc);
	// debug
//	cairo_set_source_rgba (dragging_dc, 1, 0, 0, .3f);
//	cairo_paint(dragging_dc);

	// paint piece on it - [added since we now paint to dragging_layer]
	cairo_restore(dragging_dc);
	cairo_set_source_surface(dragging_dc, anim->piece->surf, xx-wi/16, yy-hi/16);
	cairo_rectangle(dragging_dc, floor(xx-wi/16), floor(yy-hi/16), ceil(ww), ceil(hh));
	cairo_clip(dragging_dc);
	cairo_paint(dragging_dc);
	// debug
//	cairo_set_source_rgba (dragging_dc, 0, 1, 0, .3f);
//	cairo_paint(dragging_dc);
	cairo_destroy(dragging_dc);

	// paint buffer surface with dragging background
	cairo_t *cache_dc = cairo_create(cache_layer);
	cairo_set_source_surface (cache_dc, dragging_background, 0.0f, 0.0f);
	cairo_rectangle(cache_dc, floor(prev_x-wi/16), floor(prev_y-hi/16), ceil(ww), ceil(hh));
	cairo_rectangle(cache_dc, floor(xx-wi/16), floor(yy-hi/16), ceil(ww), ceil(hh));
	cairo_clip(cache_dc);
	cairo_paint(cache_dc);

	// paint animated piece at new position - [removed since we now paint to dragging_layer]
	//	cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
	//	cairo_set_source_surface (cache_dc, anim->piece->surf, xx-wi/16.0f, yy-hi/16.0f);
	//	cairo_paint(cache_dc);

	// If a piece is being dragged and overlaps with the animation, repaint the dragged piece above to cache layer
	if (mouse_dragged_piece != NULL && g_atomic_int_get(&moveit_flag)) {
		int dragged_x = g_atomic_int_get(&dragging_prev_x);
		int dragged_y = g_atomic_int_get(&dragging_prev_y);
		//FIXME: better lock access to mouse_dragged_piece (this could segfault otherwise)
		cairo_set_source_surface (cache_dc, mouse_dragged_piece->surf, dragged_x-wi/16.0f, dragged_y-hi/16.0f);
		cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
		cairo_paint(cache_dc);
	}

	cairo_destroy(cache_dc);


	// If the board isn't drawable we're probably exiting
	// free up allocated memory and stop animation
	if (!GDK_IS_DRAWABLE(board->window)) {
		debug("Aborting animation.\n");
		free_anim_data(anim);
		gdk_threads_leave();
		return FALSE;
	}
	cairo_t *cdr = gdk_cairo_create (board->window);
	cairo_rectangle(cdr, floor(xx-wi/16.0f), floor(yy-hi/16.0f), ceil(ww), ceil(hh));
	cairo_rectangle(cdr, floor(prev_x-wi/16.0f), floor(prev_y-hi/16.0f), ceil(ww), ceil(hh));
	cairo_clip(cdr);

	// apply buffered surface to cr (NB: cr is clipped)
	cairo_set_operator (cdr, CAIRO_OPERATOR_OVER);
	cairo_set_source_surface (cdr, cache_layer, 0.0f, 0.0f);
	cairo_paint(cdr);

	// debug
	//cairo_set_source_rgba (cdr, 1, 0, 0, .3f);
	//cairo_paint(cdr);

	cairo_destroy(cdr);

	anim->step_index++;
	if (anim->step_index >= anim->n_plots) { // final step

		// clean last step from dragging background
		cairo_t *dragging_dc = cairo_create(dragging_background);
		cairo_rectangle(dragging_dc, floor(xx-wi/16), floor(yy-hi/16), ceil(ww), ceil(hh));
		cairo_clip(dragging_dc);
		paint_layers(dragging_dc);
		cairo_destroy(dragging_dc);

		// handle special moves and eaten pieces now

		update_pieces_surface(wi, hi, anim->old_col, anim->old_row, anim->piece);

		cairo_t *cache_dc = cairo_create(cache_layer);

		// repaint destination square
		cairo_rectangle(cache_dc, floor(xx-wi/16.0f), floor(yy-hi/16.0f), ceil(ww), ceil(hh));
		cairo_set_source_surface(cache_dc, layer_0, 0.0f, 0.0f);
		cairo_set_operator (cache_dc, CAIRO_OPERATOR_SOURCE);
		cairo_fill_preserve(cache_dc);
		cairo_set_source_surface(cache_dc, layer_1, 0.0f, 0.0f);
		cairo_set_operator (cache_dc, CAIRO_OPERATOR_OVER);
		// If a piece is being dragged and overlaps with the animation final step
		// repaint the dragged piece above to cache layer
		if (mouse_dragged_piece != NULL && g_atomic_int_get(&moveit_flag)) {
			cairo_fill_preserve(cache_dc);
			int dragged_x = g_atomic_int_get(&dragging_prev_x);
			int dragged_y = g_atomic_int_get(&dragging_prev_y);
			//FIXME: better lock access to mouse_dragged_piece (this could segfault otherwise)
			cairo_set_source_surface (cache_dc, mouse_dragged_piece->surf, dragged_x-wi/16.0f, dragged_y-hi/16.0f);
			cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
		}
		cairo_fill(cache_dc);


		// if was castle move, handle rook
		int oc = -1;
		int or = -1;
		int nc = -1;
		int nr = -1;
		int rook_xy[2];
		if (anim->move_result > 0 && anim->move_result & CASTLE) {
			switch (anim->move_result & MOVE_DETAIL_MASK) {
				case W_CASTLE_LEFT:
					oc = 0;
					or = 0;
					nc = 3;
					nr = 0;
					break;
				case W_CASTLE_RIGHT:
					oc = 7;
					or = 0;
					nc = 5;
					nr = 0;
					break;
				case B_CASTLE_LEFT:
					oc = 0;
					or = 7;
					nc = 3;
					nr = 7;
					break;
				case B_CASTLE_RIGHT:
					oc = 7;
					or = 7;
					nc = 5;
					nr = 7;
					break;
				default:
					// Bug if it happens
					break;
			}
			update_pieces_surface_by_loc(wi, hi, oc, or, nc, nr);

			// repaint rook source square
			loc_to_xy(oc, or, rook_xy, wi, hi);
			cairo_rectangle(cache_dc, rook_xy[0]-wi/16.0f, rook_xy[1]-hi/16.0f, ww, hh);
			cairo_set_operator (cache_dc, CAIRO_OPERATOR_SOURCE);
			cairo_set_source_surface(cache_dc, layer_0, 0.0f, 0.0f);
			//cairo_fill(cache_dc);
			// If a piece is being dragged and overlaps with the animation final step
			// repaint the dragged piece above to cache layer
			if (mouse_dragged_piece != NULL && g_atomic_int_get(&moveit_flag)) {
				cairo_fill_preserve(cache_dc);
				int dragged_x = g_atomic_int_get(&dragging_prev_x);
				int dragged_y = g_atomic_int_get(&dragging_prev_y);
				//FIXME: better lock access to mouse_dragged_piece (this could segfault otherwise)
				cairo_set_source_surface (cache_dc, mouse_dragged_piece->surf, dragged_x-wi/16.0f, dragged_y-hi/16.0f);
				cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
				cairo_fill(cache_dc);
			}
			else {
				cairo_fill(cache_dc);
			}


			// repaint rook destination square
			loc_to_xy(nc, nr, rook_xy, wi, hi);
			cairo_rectangle(cache_dc, rook_xy[0]-wi/16.0f, rook_xy[1]-hi/16.0f, ww, hh);
			cairo_set_source_surface(cache_dc, layer_0, 0.0f, 0.0f);
			cairo_set_operator (cache_dc, CAIRO_OPERATOR_SOURCE);
			cairo_fill_preserve(cache_dc);
			cairo_set_source_surface(cache_dc, layer_1, 0.0f, 0.0f);
			cairo_set_operator (cache_dc, CAIRO_OPERATOR_OVER);
			//cairo_fill(cache_dc);
			// If a piece is being dragged and overlaps with the animation final step
			// repaint the dragged piece above to cache layer
			if (mouse_dragged_piece != NULL && g_atomic_int_get(&moveit_flag)) {
				cairo_fill_preserve(cache_dc);
				int dragged_x = g_atomic_int_get(&dragging_prev_x);
				int dragged_y = g_atomic_int_get(&dragging_prev_y);
				//FIXME: better lock access to mouse_dragged_piece (this could segfault otherwise)
				cairo_set_source_surface (cache_dc, mouse_dragged_piece->surf, dragged_x-wi/16.0f, dragged_y-hi/16.0f);
				cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
				cairo_fill(cache_dc);
			}
			else {
				cairo_fill(cache_dc);
			}

		}

		// handle en-passant
		int pawn_xy[2];
		if (anim->move_result > 0 && anim->move_result & EN_PASSANT) {
			loc_to_xy(anim->new_col, anim->new_row + (anim->piece->colour ? 1 : -1), pawn_xy, wi, hi);
			// repaint square where eaten pawn was
			cairo_rectangle(cache_dc, pawn_xy[0]-wi/16.0f, pawn_xy[1]-hi/16.0f, ww, hh);
			cairo_set_operator (cache_dc, CAIRO_OPERATOR_SOURCE);

			cairo_set_source_surface(cache_dc, layer_0, 0.0f, 0.0f);
			cairo_fill(cache_dc);
			// DEBUG
			//cairo_rectangle(cache_dc, pawn_xy[0]-wi/16.0f, pawn_xy[1]-hi/16.0f, ww, hh);
			//cairo_set_source_rgba(cache_dc, 1, 0, 0, .7f);
			//cairo_set_operator (cache_dc, CAIRO_OPERATOR_OVER);
			//cairo_fill(cache_dc);

			kill_piece_from_surface(wi, hi, anim->new_col, anim->new_row + (anim->piece->colour ? 1 : -1));
		}

		// handle promote
		if (anim->move_result > 0 && anim->move_result & PROMOTE && anim->move_source == AUTO_SOURCE) {
			debug("Promote from anim last step\n");
			to_promote = anim->piece;
			delay_from_promotion = FALSE;
			choose_promote(anim->promo_type, TRUE, anim->old_col, anim->old_row, anim->new_col, anim->new_row, FALSE);
		}

		cairo_t *cdr = gdk_cairo_create (board->window);

		if (highlight_last_move) {
			// de-highlight previous move
			if (lm_source_col > -1) {
				/* clean out old highlight surface */
				init_highlight_surface(wi, hi);
				cairo_t *drag_cr = cairo_create(dragging_background);
				de_highlight_square(drag_cr, lm_source_col, lm_source_row, wi, hi);
				de_highlight_square(drag_cr, lm_dest_col, lm_dest_row, wi, hi);
				de_highlight_square(cache_dc, lm_source_col, lm_source_row, wi, hi);
				de_highlight_square(cache_dc, lm_dest_col, lm_dest_row, wi, hi);

				// Special clipping
				int xy[2];
				loc_to_xy(lm_source_col, lm_source_row, xy, wi, hi);
				cairo_rectangle(cdr, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(wi/8.0f+1), ceil(hi/8.0f+1));
				loc_to_xy(lm_dest_col, lm_dest_row, xy, wi, hi);
				cairo_rectangle(cdr, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(wi/8.0f+1), ceil(hi/8.0f+1));

				cairo_destroy(drag_cr);
			}
			lm_source_col = anim->old_col;
			lm_source_row = anim->old_row;
			lm_dest_col = anim->new_col;
			lm_dest_row = anim->new_row;

			// paint to highlight layer
			cairo_t *high_cr = cairo_create(layer_2);
			highlight_square(high_cr, lm_source_col, lm_source_row, 0, 255, 0, 1, wi, hi);
			highlight_square(high_cr, lm_dest_col, lm_dest_row, 0, 255, 0, 1, wi, hi);
			cairo_destroy(high_cr);

			// update cache surface (used for scaling)
			apply_highlighted_square_over(cache_dc, lm_source_col, lm_source_row, wi, hi);
			apply_highlighted_square_over(cache_dc, lm_dest_col, lm_dest_row, wi, hi);

			// update dragging surface
			cairo_t *drag_cr = cairo_create(dragging_background);
			apply_highlighted_square_over(drag_cr, lm_source_col, lm_source_row, wi, hi);
			apply_highlighted_square_over(drag_cr, lm_dest_col, lm_dest_row, wi, hi);
			cairo_destroy(drag_cr);

			// Special clipping
			int xy[2];
			loc_to_xy(lm_source_col, lm_source_row, xy, wi, hi);
			cairo_rectangle(cdr, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(wi/8.0f+1), ceil(hi/8.0f+1));
			loc_to_xy(lm_dest_col, lm_dest_row, xy, wi, hi);
			cairo_rectangle(cdr, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(wi/8.0f+1), ceil(hi/8.0f+1));
		}

		// destroy buffer drawing context
		cairo_destroy(cache_dc);

		// clip cr to repaint only needed squares
		cairo_rectangle(cdr, floor(xx-wi/16.0f), floor(yy-hi/16.0f), ceil(ww), ceil(hh));
		// Add special clipping squares in case of castling
		if (anim->move_result > 0 && anim->move_result & CASTLE) {
			loc_to_xy(oc, or, rook_xy, wi, hi);
			cairo_rectangle(cdr, floor(rook_xy[0]-wi/16.0f), floor(rook_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
			loc_to_xy(nc, nr, rook_xy, wi, hi);
			cairo_rectangle(cdr, floor(rook_xy[0]-wi/16.0f), floor(rook_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
		}

		// Add special clipping squares in case of en-passant
		if (anim->move_result > 0 && anim->move_result & EN_PASSANT) {
			loc_to_xy(anim->new_col, anim->new_row + (anim->piece->colour ? 1 : -1), pawn_xy, wi, hi);
			cairo_rectangle(cdr, floor(pawn_xy[0]-wi/16.0f), floor(pawn_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
		}

		// Actual clip
		cairo_clip(cdr);

		cairo_set_source_surface(cdr, cache_layer, 0.0f, 0.0f);
		cairo_set_operator (cdr, CAIRO_OPERATOR_OVER);
		//cairo_set_operator (cdr, CAIRO_OPERATOR_SOURCE);
		cairo_paint(cdr);

		// debug : uncomment the following to highlight repainted areas
		//cairo_set_source_rgba (cdr, 0.0f, 1.0f, 0.0f, .75f);
		//cairo_paint(cdr);

		cairo_destroy(cdr);

		restore_dragging_background(anim->piece, anim->move_result, wi, hi);

		/* LEAVE THREADS */
		gdk_threads_leave();

		if (!anim->killed_by) {
			g_hash_table_remove(anims_map, anim->piece);
		}
		free_anim_data(anim);
		return FALSE;
	}
	/* LEAVE THREADS */
	gdk_threads_leave();
	return TRUE;

}




gboolean auto_move(chess_piece *piece, int new_col, int new_row, int check_legality, int move_source) {

	int wi = old_wi;
	int hi = old_hi;

	int old_col, old_row;

	old_col = piece->pos.column;
	old_row = piece->pos.row;

	gboolean lock_threads = !(move_source == MANUAL_SOURCE);

	/* handle special case when auto moved piece is being dragged by user */
	if (piece == mouse_dragged_piece) {
		debug("handling case when auto moved piece is being dragged by user\n");
		g_atomic_int_set(&moveit_flag, 0);

		if (lock_threads) {
			gdk_threads_enter();
		}

		mouse_dragged_piece = NULL;

		// repaint square from last dragging step
		cairo_t *cache_dc = cairo_create(cache_layer);
		clean_last_drag_step(cache_dc, wi, hi);
		cairo_destroy(cache_dc);

		cairo_t *cdr = gdk_cairo_create (board->window);

		double ww = wi/8.0f;
		double hh = hi/8.0f;
		// clip cr to repaint only needed square
		cairo_rectangle(cdr, floor(dragging_prev_x-wi/16.0f), floor(dragging_prev_y-hi/16.0f), ceil(ww), ceil(hh));

		// Actual clip
		cairo_clip(cdr);

		cairo_set_source_surface(cdr, cache_layer, 0.0f, 0.0f);
		cairo_set_operator (cdr, CAIRO_OPERATOR_OVER);
		cairo_paint(cdr);

		if (lock_threads) {
			gdk_threads_leave();
		}


	}

	if (lock_threads) {
		gdk_threads_enter();
	}
	update_dragging_background(piece, wi, hi);
	if (lock_threads) {
		gdk_threads_leave();
	}


	// actual move
	int move_result = move_piece(piece, new_col, new_row, check_legality, move_source, last_san_move, whose_turn, white_set, black_set, lock_threads);

	if (move_result >= 0) {

		// send move ASAP
		if (move_source == MANUAL_SOURCE && !delay_from_promotion) {
			char ics_mv[MOVE_BUFF_SIZE];
			if (move_result & PROMOTE) {
				sprintf(ics_mv, "%c%c%c%c=%c\n",
						'a'+old_col, '1'+old_row, 'a'+new_col, '1'+new_row,
						type_to_char(piece->type));
			}
			else {
				sprintf(ics_mv, "%c%c%c%c\n",
						'a'+old_col, '1'+old_row, 'a'+new_col, '1'+new_row);
			}
			send_to_ics(ics_mv);
		}

		if (!delay_from_promotion) {
			check_ending_clause();
			plys_list_append_ply(main_list, ply_new(p_old_col, p_old_row, new_col, new_row, NULL, last_san_move));
			insert_san_move(last_san_move, lock_threads);

			/* update eco */
			update_eco_tag(lock_threads);
		}


		int n_xy[2];
		int o_xy[2];
		loc_to_xy(old_col, old_row, o_xy, wi, hi);
		loc_to_xy(new_col, new_row, n_xy, wi, hi);

		double points_to_plot = sqrt( pow(abs(n_xy[0]-o_xy[0]), 2) + pow(abs(n_xy[1]-o_xy[1]), 2));
		points_to_plot *= 5.0f*16.0f/(wi+hi);
		//points_to_plot *= 10.0f*5.0f*16.0f/(wi+hi);
		//printf("POINTS TO PLOT == %f, wi %d, hi %d\n", points_to_plot, wi, hi);
		if (points_to_plot < 12) {
			points_to_plot = 12;
		}
		else if (points_to_plot < 16) {
			points_to_plot = 16;
		}
		else if (points_to_plot > 22) {
			points_to_plot = 22;
		}

		int mid[2];
		if (piece->type != W_KNIGHT && piece->type != B_KNIGHT) {
			mid[0] = (n_xy[0]+o_xy[0])/2.0;
			mid[1] = (n_xy[1]+o_xy[1])/2.0;
		}
		else {
			points_to_plot = 14;
			if (abs(n_xy[0]-o_xy[0]) > abs(n_xy[1]-o_xy[1])) {
				// long step along X axis
				mid[0] = n_xy[0] + ( n_xy[0] > o_xy[0] ? -1 : 1) * wi/8.0;
				mid[1] = o_xy[1];
			}
			else {
				// long step along Y axis
				mid[0] = o_xy[0];
				mid[1] = n_xy[1] + (n_xy[1] > o_xy[1] ? -1 : 1) * hi/8.0;
			}
		}

		//points_to_plot *= 2.0f;
		//points_to_plot /= 3.0f;

		int **anim_steps;
		// this will be freed when we free the anim structure!

		anim_steps = malloc(ANIM_SIZE*sizeof(int*));
		int i;
		for (i=0; i<ANIM_SIZE; i++) {
			anim_steps[i] = malloc(2*sizeof(int));
		}
		int n_anim_steps;

		plot_coords(o_xy, mid, n_xy, points_to_plot, anim_steps, &n_anim_steps);

		struct anim_data *animation = malloc(sizeof(struct anim_data));
		animation->old_col = old_col;
		animation->old_row = old_row;
		animation->new_col = new_col;
		animation->new_row = new_row;
		animation->move_result = move_result;
		if (move_result & PROMOTE) {
			animation->promo_type = promo_type;
			if (move_source != MANUAL_SOURCE) {
				logical_promote(promo_type);
			}

		}
		animation->piece = piece;
		animation->plots = anim_steps;
		animation->n_plots = n_anim_steps;
		animation->step_index = 1;
		animation->move_source = move_source;
		animation->killed_by = KILLED_BY_NONE;

		struct anim_data *old_anim = get_anim_for_piece(animation->piece);
		if (old_anim) {
			old_anim->killed_by = KILLED_BY_OTHER_ANIMATION_SAME_PIECE;
		}

		if (move_result & PIECE_TAKEN) {
			struct anim_data *killed_anim = get_anim_for_piece(last_piece_taken);
			if (killed_anim) {
				killed_anim->killed_by = KILLED_BY_OTHER_ANIMATION_TAKING;
			}
		}

		g_hash_table_insert(anims_map, animation->piece, animation);

		g_timeout_add(1000/60, animate_one_step, animation);
//		g_timeout_add(1000/3, animate_one_step, animation);
//		g_timeout_add(1000/120, animate_one_step, animation);
		return TRUE;
	}

	restore_dragging_background(piece, -1, wi, hi );

	return FALSE;

}

void init_dragging_background(int wi, int hi) {

	cairo_t *drag_dc;

	cairo_surface_destroy (dragging_background);
	dragging_background = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, wi, hi);
	drag_dc = cairo_create(dragging_background);

	paint_layers(drag_dc);

	cairo_destroy (drag_dc);

}

/* remove passed piece from dragging surface */
static void update_dragging_background(chess_piece *piece, int wi, int hi) {

	cairo_t *drag_dc;

	double ww = wi/8.0f;
	double hh = hi/8.0f;
	int xy[2];

	piece_to_xy(piece, xy, wi, hi);

	drag_dc = cairo_create(dragging_background);

	/* Repaint square from which piece originated with layer0 (the board)
	 * which means we remove the dragged piece */
	cairo_rectangle(drag_dc, xy[0]-wi/16.0f, xy[1]-hi/16.0f, ww, hh);
	cairo_set_source_surface (drag_dc, layer_0, 0.0f, 0.0f);
	cairo_fill_preserve(drag_dc);
	cairo_set_source_surface (drag_dc, layer_2, 0.0f, 0.0f);
	cairo_fill(drag_dc);
	cairo_destroy (drag_dc);

}

static void restore_dragging_background(chess_piece *piece, int move_result, int wi, int hi) {

	cairo_t *drag_dc;

	double ww = wi/8.0f;
	double hh = hi/8.0f;
	int xy[2];

	piece_to_xy(piece, xy, wi, hi);

	drag_dc = cairo_create(dragging_background);

	/* Repaint square to where piece landed */
	cairo_rectangle(drag_dc, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(ww), ceil(hh));
	if (move_result > 0 && move_result & PIECE_TAKEN) { // need to clean out dead piece so repaint whole square from scratch
		cairo_set_source_surface(drag_dc, layer_0, 0.0f, 0.0f);
		cairo_set_operator (drag_dc, CAIRO_OPERATOR_SOURCE);
		cairo_fill_preserve(drag_dc);

		// debug
//		cairo_set_source_rgba (drag_dc, 1, 0, 0, .5f);
//		cairo_set_operator (drag_dc, CAIRO_OPERATOR_OVER);
//		cairo_fill_preserve(drag_dc);
	}

	cairo_set_source_surface (drag_dc, layer_1, 0.0f, 0.0f);
	cairo_set_operator (drag_dc, CAIRO_OPERATOR_OVER);
	cairo_fill_preserve(drag_dc);

	cairo_set_source_surface (drag_dc, layer_2, 0.0f, 0.0f);
//	cairo_fill_preserve(drag_dc);
//	cairo_set_source_rgba (drag_dc, 1, 0, 0, .5f);
	cairo_fill(drag_dc);

	// handle castle
	if (move_result > 0 && move_result & CASTLE) { // need to clean out old rooks pos
		int oc = -1, or = -1, nc = -1, nr = -1;
		int rook_xy[2];
		switch (move_result & MOVE_DETAIL_MASK) {
		case W_CASTLE_LEFT:
			oc = 0;
			or = 0;
			nc = 3;
			nr = 0;
			break;
		case W_CASTLE_RIGHT:
			oc = 7;
			or = 0;
			nc = 5;
			nr = 0;
			break;
		case B_CASTLE_LEFT:
			oc = 0;
			or = 7;
			nc = 3;
			nr = 7;
			break;
		case B_CASTLE_RIGHT:
			oc = 7;
			or = 7;
			nc = 5;
			nr = 7;
			break;
		default:
			break;
		}

		// repaint rook source square
		loc_to_xy(oc, or, rook_xy, wi, hi);
		cairo_rectangle(drag_dc, floor(rook_xy[0]-wi/16.0f), floor(rook_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
		cairo_set_operator (drag_dc, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_surface(drag_dc, layer_0, 0.0f, 0.0f);
		cairo_fill(drag_dc);

		// repaint rook destination square
		loc_to_xy(nc, nr, rook_xy, wi, hi);
		cairo_rectangle(drag_dc, floor(rook_xy[0]-wi/16.0f), floor(rook_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
		cairo_set_source_surface(drag_dc, layer_0, 0.0f, 0.0f);
		cairo_set_operator (drag_dc, CAIRO_OPERATOR_SOURCE);
		cairo_fill_preserve(drag_dc);
		cairo_set_source_surface(drag_dc, layer_1, 0.0f, 0.0f);
		cairo_set_operator (drag_dc, CAIRO_OPERATOR_OVER);
		cairo_fill_preserve(drag_dc);
		cairo_set_source_surface(drag_dc, layer_2, 0.0f, 0.0f);
		cairo_fill(drag_dc);
	}

	// handle en-passant
	int pawn_xy[2];
	if (move_result > 0 && move_result & EN_PASSANT) {
		// repaint square where eaten pawn was
		loc_to_xy(piece->pos.column, piece->pos.row +(whose_turn ? - 1 : 1), pawn_xy, wi, hi);
		cairo_rectangle(drag_dc, pawn_xy[0]-wi/16.0f, pawn_xy[1]-hi/16.0f, ww, hh);
		cairo_set_operator (drag_dc, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_surface(drag_dc, layer_0, 0.0f, 0.0f);
		cairo_fill_preserve(drag_dc);
		cairo_set_operator (drag_dc, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface(drag_dc, layer_2, 0.0f, 0.0f);
		cairo_fill(drag_dc);
	}

	cairo_destroy (drag_dc);

}

// repaint square from last dragging step
// NB: allow one pixel around square in case of rounding
// This is only a convenience method and is not thread safe
static void clean_last_drag_step(cairo_t *cdc, double wi, double hi) {
	double ww = wi/8.0f;
	double hh = hi/8.0f;
	cairo_save(cdc);
	cairo_rectangle(cdc, dragging_prev_x-1-wi/16.0f, dragging_prev_y-1-hi/16.0f, ww+2, hh+2);
	cairo_clip(cdc);
	cairo_set_source_surface(cdc, layer_0, 0.0f, 0.0f);
	cairo_set_operator (cdc, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cdc);
	cairo_set_source_surface(cdc, layer_1, 0.0f, 0.0f);
	cairo_set_operator (cdc, CAIRO_OPERATOR_OVER);
	cairo_paint(cdc);
	cairo_set_source_surface(cdc, layer_2, 0.0f, 0.0f);
	cairo_paint(cdc);
	cairo_restore(cdc);
}

void handle_button_release(void) {

	int new_x = g_atomic_int_get(&last_release_x);
	int new_y = g_atomic_int_get(&last_release_y);

	int ij[2];
	//double wi = pWidget->allocation.width;
	//double hi = pWidget->allocation.height;
	double wi = board->allocation.width;
	double hi = board->allocation.height;

	int move_result = -1;

	if (mouse_dragged_piece != NULL) {

		g_atomic_int_set(&moveit_flag, 0);
		g_atomic_int_set(&more_events_flag, 0);

		xy_to_loc(new_x, new_y, ij, wi, hi);

		if (highlight_moves) {
			highlightPotentialMoves(board, mouse_dragged_piece, wi, hi, FALSE);
		}

		// Save old position
		p_old_col = mouse_dragged_piece->pos.column;
		p_old_row = mouse_dragged_piece->pos.row;

		gboolean piece_moved = false;
		// if piece NOT moved, don't bother trying the move
		if ( p_old_row != ij[1] || p_old_col != ij[0] ) {

			// if I'm not allowed to move it, don't bother
			if (can_i_move_piece(mouse_dragged_piece)) {

				// try the move
				move_result = move_piece(mouse_dragged_piece, ij[0], ij[1], 1, MANUAL_SOURCE, last_san_move, whose_turn, white_set, black_set, FALSE);

				if (move_result >= 0) {

					// send move ASAP
					if (!delay_from_promotion) {
						char ics_mv[MOVE_BUFF_SIZE];
						if (move_result & PROMOTE) {
							sprintf(ics_mv, "%c%c%c%c=%c\n",
									'a'+p_old_col, '1'+p_old_row, 'a'+ij[0], '1'+ij[1],
									type_to_char(mouse_dragged_piece->type));
						}
						else {
							sprintf(ics_mv, "%c%c%c%c\n",
									'a'+p_old_col, '1'+p_old_row, 'a'+ij[0], '1'+ij[1]);
						}
						send_to_ics(ics_mv);
						if (crafty_mode) {
							write_to_crafty(ics_mv);
						}
					}

					piece_moved = true;

					// check if killed piece was animated
					if (move_result & PIECE_TAKEN) {
							struct anim_data *killed_anim = get_anim_for_piece(last_piece_taken);
							if (killed_anim) {
								killed_anim->killed_by = KILLED_BY_INSTANT_MOVE_TAKING;
							}
					}
					//debug("SAN: %s\n", san_move);
					if (!delay_from_promotion) {
						check_ending_clause();
						plys_list_append_ply(main_list, ply_new(p_old_col, p_old_row, ij[0], ij[1], NULL, last_san_move));
						insert_san_move(last_san_move, FALSE);
						/* update eco - we're already inside threads lock */
						update_eco_tag(FALSE);
					}

					// only redraw pieces we need to redraw!
					update_pieces_surface(wi, hi, p_old_col, p_old_row, mouse_dragged_piece);
				}
			}
		}
		if(!piece_moved) {
			restore_piece_to_surface(wi, hi, mouse_dragged_piece);
		}

		ij[0] = mouse_dragged_piece->pos.column;
		ij[1] = mouse_dragged_piece->pos.row;


		int xy[2];
		double ww = wi/8.0f;
		double hh = hi/8.0f;
		loc_to_xy(ij[0], ij[1], xy, wi, hi);

		cairo_t *cache_dc = cairo_create(cache_layer);

		// repaint square from last dragging step
		clean_last_drag_step(cache_dc, wi, hi);

		// repaint destination square
		cairo_rectangle(cache_dc, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(ww), ceil(hh));
		cairo_set_source_surface(cache_dc, layer_0, 0.0f, 0.0f);
		cairo_set_operator (cache_dc, CAIRO_OPERATOR_SOURCE);
		cairo_fill_preserve(cache_dc);
		cairo_set_source_surface(cache_dc, layer_1, 0.0f, 0.0f);
		cairo_set_operator (cache_dc, CAIRO_OPERATOR_OVER);
		cairo_fill_preserve(cache_dc);
		cairo_set_source_surface(cache_dc, layer_2, 0.0f, 0.0f);
		cairo_fill(cache_dc);

		// if was castle move, handle rook
		int oc = -1;
		int or = -1;
		int nc = -1;
		int nr = -1;
		int rook_xy[2];
		if (move_result > 0 && move_result & CASTLE) {
			switch (move_result & MOVE_DETAIL_MASK) {
			case W_CASTLE_LEFT:
				oc = 0;
				or = 0;
				nc = 3;
				nr = 0;
				break;
			case W_CASTLE_RIGHT:
				oc = 7;
				or = 0;
				nc = 5;
				nr = 0;
				break;
			case B_CASTLE_LEFT:
				oc = 0;
				or = 7;
				nc = 3;
				nr = 7;
				break;
			case B_CASTLE_RIGHT:
				oc = 7;
				or = 7;
				nc = 5;
				nr = 7;
				break;
			default:
				// Bug if it happens
				break;
			}
			update_pieces_surface_by_loc(wi, hi, oc, or, nc, nr);

			// repaint rook source square
			loc_to_xy(oc, or, rook_xy, wi, hi);
			cairo_rectangle(cache_dc, floor(rook_xy[0]-wi/16.0f), floor(rook_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
			cairo_set_source_surface(cache_dc, layer_0, 0.0f, 0.0f);
			cairo_set_operator (cache_dc, CAIRO_OPERATOR_SOURCE);
			cairo_fill(cache_dc);

			// repaint rook destination square
			loc_to_xy(nc, nr, rook_xy, wi, hi);
			cairo_rectangle(cache_dc, floor(rook_xy[0]-wi/16.0f), floor(rook_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
			cairo_set_source_surface(cache_dc, layer_0, 0.0f, 0.0f);
			cairo_set_operator (cache_dc, CAIRO_OPERATOR_SOURCE);
			cairo_fill_preserve(cache_dc);
			cairo_set_source_surface(cache_dc, layer_1, 0.0f, 0.0f);
			cairo_set_operator (cache_dc, CAIRO_OPERATOR_OVER);
			cairo_fill(cache_dc);
		}

		int pawn_xy[2];
		if (move_result > 0 && move_result & EN_PASSANT) {
			loc_to_xy(ij[0], ij[1] + (mouse_dragged_piece->colour ? 1 : -1), pawn_xy, wi, hi);
			// repaint square where eaten pawn was
			cairo_rectangle(cache_dc, pawn_xy[0]-wi/16.0f, pawn_xy[1]-hi/16.0f, ww, hh);
			cairo_set_source_surface(cache_dc, layer_0, 0.0f, 0.0f);
			cairo_set_operator (cache_dc, CAIRO_OPERATOR_SOURCE);
			cairo_fill(cache_dc);
			kill_piece_from_surface(wi, hi, ij[0], ij[1] + (whose_turn ? -1 : 1));
		}


		// destroy buffer drawing context
		cairo_destroy(cache_dc);

		cairo_t *cdr = gdk_cairo_create (board->window);
		// clip cr to repaint only needed squares
		cairo_rectangle(cdr, dragging_prev_x-wi/16.0f, dragging_prev_y-hi/16.0f, ww, hh);
		cairo_rectangle(cdr, floor(xy[0]-wi/16.0f), floor(xy[1]-hi/16.0f), ceil(ww), ceil(hh));

		// Add special clipping squares in case of castling
		if (move_result > 0 && move_result & CASTLE) {
			loc_to_xy(oc, or, rook_xy, wi, hi);
			cairo_rectangle(cdr, floor(rook_xy[0]-wi/16.0f), floor(rook_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
			loc_to_xy(nc, nr, rook_xy, wi, hi);
			cairo_rectangle(cdr, floor(rook_xy[0]-wi/16.0f), floor(rook_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
		}

		// Add special clipping squares in case of en-passant
		if (move_result > 0 && move_result & EN_PASSANT) {
			loc_to_xy(ij[0], ij[1] + (mouse_dragged_piece->colour ? 1 : -1), pawn_xy, wi, hi);
			cairo_rectangle(cdr, floor(pawn_xy[0]-wi/16.0f), floor(pawn_xy[1]-hi/16.0f), ceil(ww), ceil(hh));
		}

		// Actual clip
		cairo_clip(cdr);

		cairo_set_source_surface(cdr, cache_layer, 0.0f, 0.0f);
		cairo_set_operator (cdr, CAIRO_OPERATOR_OVER);
		cairo_paint(cdr);

		// debug : uncomment the following to highlight repainted areas
		//cairo_set_source_rgba (cdr, 0.0f, 1.0f, 0.0f, .75f);
		//cairo_paint(cdr);

		cairo_destroy(cdr);

		restore_dragging_background(mouse_dragged_piece, move_result, wi, hi );

		mouse_dragged_piece = NULL;
	}


	/* Here are the 4 rules for click-(de)selecting a piece:
	 *
	 * 1. press and released on same square and piece wasn't selected before : selecting
	 * 2. press and released on same square but piece WAS selected before: deselecting
	 * 3. press and released on different square: deselecting
	 * 4. no piece selected OR valid move occurred: deselecting
	 *
	 *    NOTE: in the deselecting cases, we don't need to do anything
	 *    graphics related as this has been done in the on_press step */
	if (mouse_clicked_piece != NULL && move_result < 0) {
		chess_square *square = xy_to_square(new_x, new_y, wi, hi);
		chess_piece* piece = square->piece;
		if (mouse_clicked_piece == piece) {

			xy_to_loc(new_x, new_y, ij, wi, hi);

			//cairo_t *main_cr = gdk_cairo_create(pWidget->window);

			// only highlight if it's a different piece from last click
			// (if user clicked on same piece again she wants to toggle it off)
			if (mouse_clicked[0] != ij[0] || mouse_clicked[1] != ij[1]) {

				// paint to main
				cairo_t *main_cr = gdk_cairo_create(board->window);
				highlight_square(main_cr, ij[0], ij[1], 1, 1, 0, 1, wi, hi);
				cairo_destroy(main_cr);

				// paint to highlight layer
				cairo_t *high_cr = cairo_create(layer_2);
				highlight_square(high_cr, ij[0], ij[1], 1, 1, 0, 1, wi, hi);
				cairo_destroy(high_cr);

				// update cache surface (used for scaling)
				cairo_t *cache_cr = cairo_create(cache_layer);
				paint_layers(cache_cr);
				cairo_destroy(cache_cr);

				// 1. click and released on same square and piece wasn't selected before : selecting
				mouse_clicked[0] = ij[0];
				mouse_clicked[1] = ij[1];
			}
			else {
				// 2. click and released on same square but piece WAS selected before: de-selecting
				mouse_clicked[0] = -1;
				mouse_clicked[1] = -1;
			}

		}
		else {
			// 3. click and released on different square: de-selecting
			mouse_clicked[0] = -1;
			mouse_clicked[1] = -1;
		}
	}
	else {
		// 4. no piece selected OR valid move occurred: de-selecting
		mouse_clicked[0] = -1;
		mouse_clicked[1] = -1;
	}
}

void handle_left_button_press(GtkWidget *pWidget, int wi, int hi, int x, int y) {
	/* clean out any previous highlight */
	if (mouse_clicked[0] >=0) {
		cairo_t *board_cr = gdk_cairo_create (pWidget->window);
		de_highlight_square(board_cr, mouse_clicked[0], mouse_clicked[1], wi, hi);
		cairo_destroy(board_cr);

		/* clean out old highlight surface */
//		init_highlight_surface(wi, hi);
		clean_highlight_surface(mouse_clicked[0], mouse_clicked[1], wi, hi);
	}

	chess_square* square = xy_to_square(x, y, wi, hi);

	if (mouse_clicked_piece != NULL && mouse_clicked[0] >= 0 && mouse_clicked[1] >= 0) {
		int ij[2];
		xy_to_loc(x, y, ij, wi, hi);
		if (auto_move(mouse_clicked_piece, ij[0], ij[1], 1, MANUAL_SOURCE) > 0) {
			mouse_clicked_piece = NULL;
			return; // only return if move was successful
		}
		// NB: when automove fails we don't return to allow user to grab the piece for dragging
	}

	if (square->piece != NULL && whose_turn == square->piece->colour && can_i_move_piece(square->piece)) {
		mouse_clicked_piece = square->piece;
	}
	else {
		mouse_clicked_piece = NULL;
	}

	if (mouse_dragged_piece != NULL) {
		g_atomic_int_set(&moveit_flag, 0);
		g_atomic_int_set(&more_events_flag, 0);
		debug("Doh! How did you manage to drag 2 pieces at once?\n");
	}
	mouse_dragged_piece = square->piece;

	if (mouse_dragged_piece != NULL) {

		kill_piece_from_surface(wi, hi, mouse_dragged_piece->pos.column, mouse_dragged_piece->pos.row);
		update_dragging_background(mouse_dragged_piece, wi, hi);

		int xy[2];
		piece_to_xy(mouse_dragged_piece, xy, wi, hi);
		dragging_prev_x = xy[0];
		dragging_prev_y = xy[1];

		g_atomic_int_set(&moveit_flag, 1);

		if (highlight_moves) {
			highlightPotentialMoves(pWidget, mouse_dragged_piece, wi, hi, TRUE);
		}
	}
}

void handle_right_button_press(GtkWidget *pWidget, int wi, int hi) {
	// Logical flip and repaint pieces layer
	flip_board(old_wi, old_hi);

	// Reconstruct cache layer
	cairo_t *cache_cr = cairo_create(cache_layer);
	paint_layers(cache_cr);
	cairo_destroy(cache_cr);

	// Update displayed board
	cairo_t *cdr = gdk_cairo_create (pWidget->window);
	cairo_set_source_surface(cdr, cache_layer, 0, 0);
	cairo_set_operator(cdr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cdr);

	init_dragging_background(old_wi, old_hi);
	cairo_destroy(cdr);
}

void handle_middle_button_press(GtkWidget *pWidget, int wi, int hi) {
	/*if ( ! playing ) {
					reset_board();
					//sleep(3);
					//auto_play_one_move(pWidget);
					auto_playing_timer = g_timeout_add(300, auto_play_one_move, pWidget);
				}*/
	//auto_play_one_move(pWidget);
	/*if (auto_play_timer) {
					g_source_remove(auto_play_timer);
					auto_play_timer = 0;
				}
				else {
					auto_play_timer = g_timeout_add(500, auto_play_one_move, pWidget);
				}*/
	//auto_play_one_ics_move(pWidget);
	//g_timeout_add(300, auto_play_one_move, pWidget);
}

void handle_flip_board(GtkWidget *pWidget) {

	gdk_threads_enter();

	// Logical flip and repaint pieces layer
	flip_board(old_wi, old_hi);

	// Reconstruct cache layer
	cairo_t *cache_cr = cairo_create(cache_layer);
	paint_layers(cache_cr);
	cairo_destroy(cache_cr);

	// This is needed because we're in a custom signal handler


	// Update displayed board
	cairo_t *cdr = gdk_cairo_create (pWidget->window);
	cairo_set_source_surface(cdr, cache_layer, 0, 0);
	cairo_set_operator(cdr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cdr);

	cairo_destroy(cdr);

	init_dragging_background(old_wi, old_hi);

	gdk_threads_leave();


	// Kludge to wake-up the window
	//g_main_context_wakeup(NULL);
}

void *process_moves(void *ptr) {
	while ( g_atomic_int_get(&running_flag) ) {

		if (g_atomic_int_get(&moveit_flag) && g_atomic_int_get(&more_events_flag)) {

			gdk_threads_enter();

			int wi = board->allocation.width;
			int hi = board->allocation.height;
			double ww = wi/8.0f;
			double hh = hi/8.0f;

			if (mouse_dragged_piece == NULL || !g_atomic_int_get(&moveit_flag)) {
				debug("Dragging animation interrupted\n");
				gdk_threads_leave();
				continue;
			}
			if (mouse_dragged_piece->dead) {
				debug("handling case when dragged piece was killed\n");
					g_atomic_int_set(&moveit_flag, 0);

						// repaint square from last dragging step
						cairo_t *cache_dc = cairo_create(cache_layer);
						clean_last_drag_step(cache_dc, wi, hi);
						cairo_destroy(cache_dc);

						cairo_t *cdr = gdk_cairo_create (board->window);

						double ww = wi/8.0f;
						double hh = hi/8.0f;
						// clip cr to repaint only needed square
						cairo_rectangle(cdr, floor(dragging_prev_x-wi/16.0f), floor(dragging_prev_y-hi/16.0f), ceil(ww), ceil(hh));

						// Actual clip
						cairo_clip(cdr);

						cairo_set_source_surface(cdr, cache_layer, 0.0f, 0.0f);
						cairo_set_operator (cdr, CAIRO_OPERATOR_OVER);
						cairo_paint(cdr);

						mouse_dragged_piece = NULL;

						gdk_threads_leave();

						continue;
			}

			/* Get coordinates from last mouse move */
			int new_x = g_atomic_int_get(&last_move_x);
			int new_y = g_atomic_int_get(&last_move_y);

			// Mark that we processed the last motion event
			g_atomic_int_set(&more_events_flag, 0);

			// double buffering using cache layer

			// Paint dragging Background to cache layer
			cairo_t *cache_dc = cairo_create(cache_layer);
			cairo_set_source_surface (cache_dc, dragging_background, 0.0f, 0.0f);
			cairo_paint(cache_dc);

			// Paint piece at new position to cache layer
			cairo_set_source_surface (cache_dc, mouse_dragged_piece->surf, new_x-wi/16.0f, new_y-hi/16.0f);
			cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
			cairo_paint(cache_dc);

			/* destroy cache_dc*/
			cairo_destroy(cache_dc);

			cairo_t *cdr = gdk_cairo_create (board->window);
			cairo_rectangle(cdr, floor(new_x-wi/16.0f), floor(new_y-hi/16.0f), ceil(ww), ceil(hh));
			cairo_rectangle(cdr, floor(dragging_prev_x-wi/16.0f), floor(dragging_prev_y-hi/16.0f), ceil(ww), ceil(hh));
			cairo_clip(cdr);

			// Apply cache surface to visible canvas
			cairo_set_operator (cdr, CAIRO_OPERATOR_OVER);
			cairo_set_source_surface (cdr, cache_layer, 0.0f, 0.0f);
			cairo_paint(cdr);

			// debug: uncomment to highlight repainted areas
			/*{
				cairo_set_source_rgba (cdr, 1.0f, 0.0f, 0.0f, .4f);
				cairo_rectangle(cdr, dragging_prev_x-wi/16.0f, dragging_prev_y-hi/16.0f, ww, hh);
				cairo_fill(cdr);
				cairo_set_source_rgba (cdr, 0.0f, 1.0f, 0.0f, .4f);
				cairo_rectangle(cdr, new_x-wi/16.0f, new_y-hi/16.0f, ww, hh);
				cairo_fill(cdr);
			}*/

			cairo_destroy(cdr);

			// FIXME: clean this whole dragging_prev_x shite
			dragging_prev_x = new_x;
			dragging_prev_y = new_y;

			gdk_threads_leave();


			/* I'm sure this was here for a reason... but now it seems useless */
			//g_main_context_wakeup(NULL);
		}
		usleep(1000000/60);
//		usleep(1000000/560);
	}
	return 0;
}

/* Generate an array of xy coordinates to animate a move from start->mid->end. */
// FIXME: review this
static void plot_coords(int start[2], int mid[2], int end[2], int points_to_plot, int **plots, int *nPlots) {
	//int denom;
	double denom;
	int n, count;
	double half_points_num = points_to_plot/2.0f;
	double k = pow(50, ((double)1.0f)/half_points_num);
	//printf("k %f\n", k);
	count = 0;

	/* add the start position to the plots */
	plots[count][0] = start[0];
	plots[count][1] = start[1];
	count++;

	/* Step in */
	denom = pow(k, half_points_num);

	for (n = 0; n < half_points_num; n++) {
		plots[count][0] = start[0] + (mid[0] - start[0]) / denom;
		plots[count][1] = start[1] + (mid[1] - start[1]) / denom;
		count ++;
		denom /= k;
	}

	/* Step out */
	denom = k;
	for (n = 0; n < half_points_num; n++) {
		plots[count][0] = end[0] - (end[0] - mid[0]) / denom;
		plots[count][1] = end[1] - (end[1] - mid[1]) / denom;
		count++;
		denom *= k;
	}

	/* add the final position to the plots */
	plots[count][0] = end[0];
	plots[count][1] = end[1];
	count++;

	*nPlots = count;
}


/* This must be called to free up the malloc'ed memory */
static void free_anim_data(struct anim_data *anim) {
	int i;
	for (i=0; i<ANIM_SIZE; i++) {
		free(anim->plots[i]);
	}
	free(anim->plots);
	free(anim);
}

enum EDGE {
	TOP,
	RIGHT,
	BOTTOM,
	LEFT
};

static void create_trapeze_path(cairo_t *dc, double x, double y, double w, double h, double line_width, enum EDGE edge) {

	switch (edge) {

	case TOP:
		cairo_move_to(dc, x, y);
		cairo_rel_line_to(dc, line_width, line_width);
		cairo_rel_line_to(dc, w-2*line_width, 0);
		cairo_rel_line_to(dc, line_width, -line_width);
		break;
	case RIGHT:
		cairo_move_to(dc, x+w, y);
		cairo_rel_line_to(dc, -line_width, line_width);
		cairo_rel_line_to(dc, 0, h-2*line_width);
		cairo_rel_line_to(dc, line_width, line_width);
		break;
	case BOTTOM:
		cairo_move_to(dc, x, y+h);
		cairo_rel_line_to(dc, line_width, -line_width);
		cairo_rel_line_to(dc, w-2*line_width, 0);
		cairo_rel_line_to(dc, line_width, line_width);
		break;
	case LEFT:
		cairo_move_to(dc, x, y);
		cairo_rel_line_to(dc, line_width, line_width);
		cairo_rel_line_to(dc, 0, h-2*line_width);
		cairo_rel_line_to(dc, -line_width, line_width);
		break;
	}

	cairo_close_path(dc);
}


/* Highlight a square insets e.g. to mark a selection */
static void highlight_square(cairo_t *dc, int col, int row, double r, double g, double b, double a, int wi, int hi) {

	// Calculate the gradient half width
	double half_line_width = ((double)wi)/(8.0f*45.0f);
	if (half_line_width < 1.0f) {
		half_line_width = 1.0f;
	}
	double line_width = 4*half_line_width;

	int xy[2];
	loc_to_xy(col, row, xy, wi, hi);

	double x,y,w,h;
	x = xy[0]-wi/16.0f;
	y = xy[1]-hi/16.0f;
	w = wi/8.0f;
	h = hi/8.0f;

	// Build gradients
	cairo_pattern_t *linpat;

	// Top Edge
	linpat = cairo_pattern_create_linear (x, y, x, y+line_width);
	cairo_pattern_add_color_stop_rgba(linpat, 0.00,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba(linpat, 0.33,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba(linpat, 1.00,  r, g, b, 0);

	create_trapeze_path(dc, x, y, w, h, line_width, TOP);
	cairo_set_source(dc, linpat);
	cairo_fill(dc);

	// Right Edge
	linpat = cairo_pattern_create_linear (x+w, y, x+w-line_width, y);
	cairo_pattern_add_color_stop_rgba (linpat, 0.00,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 0.33,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 1.00,  r, g, b, 0);

	create_trapeze_path(dc, x, y, w, h, line_width, RIGHT);
	cairo_set_source (dc, linpat);
	cairo_fill (dc);

	// Bottom Edge
	linpat = cairo_pattern_create_linear (x+w, y+h, x+w, y+h-line_width);
	cairo_pattern_add_color_stop_rgba (linpat, 0.00,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 0.33,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 1.00,  r, g, b, 0);

	create_trapeze_path(dc, x, y, w, h, line_width, BOTTOM);
	cairo_set_source (dc, linpat);
	cairo_fill (dc);

	// Left Edge
	linpat = cairo_pattern_create_linear (x, y+h, x+line_width, y+h);
	cairo_pattern_add_color_stop_rgba (linpat, 0.00,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 0.33,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 1.00,  r, g, b, 0);

	create_trapeze_path(dc, x, y, w, h, line_width, LEFT);
	cairo_set_source (dc, linpat);
	cairo_fill (dc);

	// Now fill in the gaps in case of display rounding

	// Top Left
	linpat = cairo_pattern_create_linear (x, y, x+line_width,y+line_width);
	cairo_pattern_add_color_stop_rgba (linpat, 0.00,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 0.33,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 1.00,  r, g, b, 0);

	cairo_move_to(dc, x+1, y+1);
	cairo_rel_line_to(dc, line_width-3, line_width-3);
	cairo_set_line_width(dc, 1);
	cairo_set_source(dc, linpat);
	cairo_stroke(dc);

	// Top Right
	linpat = cairo_pattern_create_linear (x+w, y, x+w-line_width, y+line_width);
	cairo_pattern_add_color_stop_rgba (linpat, 0.00,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 0.33,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 1.00,  r, g, b, 0);

	cairo_move_to(dc, x+w-1, y+1);
	cairo_rel_line_to(dc, -line_width+3, line_width-3);
	cairo_set_line_width(dc, 1);
	cairo_set_source(dc, linpat);
	cairo_stroke(dc);

	// Bottom Right
	linpat = cairo_pattern_create_linear (x+w, y+h, x+w-line_width, y+h-line_width);
	cairo_pattern_add_color_stop_rgba (linpat, 0.00,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 0.33,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 1.00,  r, g, b, 0);

	cairo_move_to(dc, x+w-1, y+h-1);
	cairo_rel_line_to(dc, -line_width+3, -line_width+3);
	cairo_set_line_width(dc, 1);
	cairo_set_source(dc, linpat);
	cairo_stroke(dc);

	// Bottom Left
	linpat = cairo_pattern_create_linear (x, y+h, x+line_width, y+h-line_width);
	cairo_pattern_add_color_stop_rgba (linpat, 0.00,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 0.33,  r, g, b, 1);
	cairo_pattern_add_color_stop_rgba (linpat, 1.00,  r, g, b, 0);

	cairo_move_to(dc, x+1, y+h-1);
	cairo_rel_line_to(dc, line_width-3, -line_width+3);
	cairo_set_line_width(dc, 1);
	cairo_set_source(dc, linpat);
	cairo_stroke(dc);

	// use this instead for plain rectangle
//	cairo_rectangle(dc, xy[0]-wi/16.0f+half_line_width, xy[1]-hi/16.0f+half_line_width, wi/8.0f-line_width, hi/8.0f-line_width);
//	cairo_set_source_rgba(dc, r, g, b, a);
//	cairo_set_line_width (dc, line_width);
//	cairo_stroke(dc);
}

static void logical_promote(int last_promote) {
	debug("Logical Promote\n");
	toggle_piece(to_promote);

	switch (last_promote) {
			case W_QUEEN:
			case B_QUEEN:
				debug("You chose Queen\n");
				to_promote->type = ( to_promote->colour ? B_QUEEN : W_QUEEN);
				break;
			case W_ROOK:
			case B_ROOK:
				debug("You chose Rook\n");
				to_promote->type = ( to_promote->colour ? B_ROOK : W_ROOK);
				break;
			case W_BISHOP:
			case B_BISHOP:
				debug("You chose Bishop\n");
				to_promote->type = ( to_promote->colour ? B_BISHOP : W_BISHOP);
				break;
			case W_KNIGHT:
			case B_KNIGHT:
				debug("You chose Knight\n");
				to_promote->type = ( to_promote->colour ? B_KNIGHT : W_KNIGHT);
				break;
			case -1:
				if (to_promote->type == W_PAWN || to_promote->type == B_PAWN) {
						last_promote = W_QUEEN;
						to_promote->type = ( to_promote->colour ? B_QUEEN : W_QUEEN);
						to_promote->surf = piecesSurf[(to_promote->colour ? B_QUEEN : W_QUEEN)];
					}
				break;
			default:
				fprintf(stderr, "%d invalid promotion choice!\n", last_promote);
				break;
		}

		// add promoted pawn from hash
		toggle_piece(to_promote);
}

void choose_promote(int last_promote, gboolean only_surfaces, int ocol, int orow, int ncol, int nrow, gboolean lock_threads) {

	debug("Promote to %d\n", last_promote);

	if (!only_surfaces) {
		logical_promote(last_promote);
	}

	switch (last_promote) {
		case W_QUEEN:
		case B_QUEEN:
			debug("You chose Queen\n");
			to_promote->surf = piecesSurf[(to_promote->colour ? B_QUEEN : W_QUEEN)];
			break;
		case W_ROOK:
		case B_ROOK:
			debug("You chose Rook\n");
			to_promote->surf = piecesSurf[(to_promote->colour ? B_ROOK : W_ROOK)];
			break;
		case W_BISHOP:
		case B_BISHOP:
			debug("You chose Bishop\n");
			to_promote->surf = piecesSurf[(to_promote->colour ? B_BISHOP: W_BISHOP)];
			break;
		case W_KNIGHT:
		case B_KNIGHT:
			debug("You chose Knight\n");
			to_promote->surf = piecesSurf[(to_promote->colour ? B_KNIGHT: W_KNIGHT)];
			break;
		default:
			fprintf(stderr, "%d invalid promotion choice!\n", last_promote);
			break;
	}

	// send the delayed move to ICS
	if (delay_from_promotion) {
		char ics_mv[MOVE_BUFF_SIZE];
		sprintf(ics_mv, "%c%c%c%c=%c\n", 'a'+ocol, '1'+orow, 'a'+ncol, '1'+nrow, type_to_char(to_promote->type));
		send_to_ics(ics_mv);

		char bufstr[8];
		memset(bufstr, 0, sizeof(bufstr));
		if (use_fig) {
			sprintf(bufstr, "=%lc", type_to_unicode_char(to_promote->type));
		}
		else {
			sprintf(bufstr, "=%c", type_to_char(to_promote->type));
		}
		strcat(last_san_move, bufstr);
		check_ending_clause();

		plys_list_append_ply(main_list, ply_new(ocol, orow, ncol, nrow, NULL, last_san_move));
		// FIXME: review this lock thread for inserting a move into list
		insert_san_move(last_san_move, FALSE);
		update_eco_tag(FALSE);
	}

	update_pieces_surface_by_loc(old_wi, old_hi, ncol, nrow, ncol, nrow);

	/* update the dragging background layer with the new piece surface */
	restore_dragging_background(to_promote, PIECE_TAKEN, old_wi, old_hi);

	// repaint that square for new piece to appear
	int xy[2];
	loc_to_xy(ncol, nrow, xy, old_wi, old_hi);
	if ( ! GTK_IS_WIDGET(board) ) {
		// killed? tough
		return;
	}

	gtk_widget_queue_draw_area(board, xy[0]-old_wi/16.0, xy[1]-old_hi/16.0, old_wi/8.0, old_hi/8.0);

}

gboolean idle_promote_chooser(gpointer trash) {
	if (!has_chosen) {
		debug("User has NOT chosen! Defaulting to Queen\n");
		choose_promote(1, FALSE, p_old_col, p_old_row, to_promote->pos.column, to_promote->pos.row, TRUE);
	}
	return FALSE;
}

void choose_promote_deactivate_handler(void *GtkWidget, gpointer value, gboolean only_surfaces) {
	g_idle_add_full(G_PRIORITY_HIGH, idle_promote_chooser, NULL, NULL);
}

void choose_promote_handler(void *GtkWidget, gpointer value) {
	has_chosen = TRUE;
	choose_promote(GPOINTER_TO_INT(value), FALSE, p_old_col, p_old_row, to_promote->pos.column, to_promote->pos.row, FALSE);
}

void reset_board(void) {
	// Need to reassign surfaces in case of promotions during previous game
	flipped = 0;
	mouse_clicked[0] = -1;
	mouse_clicked[1] = -1;
	lm_source_col = -1;
	assign_surfaces();
	draw_pieces_surface(old_wi, old_hi);
	init_dragging_background(old_wi, old_hi);
	init_highlight_surface(old_wi, old_hi);
	gtk_widget_queue_draw(GTK_WIDGET(board));
}

gboolean test_animate_random_step(gpointer data) {
	chess_piece *piece = (chess_piece*)data;
	static int prev_x1 = 200;
	static int prev_y1 = 250;

	static int prev_x2 = 300;
	static int prev_y2 = 450;

	if (mouse_dragged_piece == piece) {
		if (piece->colour) {
//			prev_x1 = dragging_prev_x;
//			prev_y1 = dragging_prev_y;
//			return TRUE;
		}
		else {
//			prev_x2 = dragging_prev_x;
//			prev_y2 = dragging_prev_y;
//			return TRUE;
		}
	}
	int xx, yy;


	double wi = (double)board->allocation.width;
	double hi = (double)board->allocation.height;

	if (piece->colour) {
		xx = prev_x1 + (rand()%7-3);
		yy = prev_y1 + (rand()%5-2);
	}
	else {
		xx = prev_x2 + (rand()%6-2);
		yy = prev_y2 + (rand()%4-1);
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec + rand());

	xx %= (int)wi;
	yy %= (int)hi;

	double ww = wi/8.0f;
	double hh = hi/8.0f;

	/* ENTER THREADS */
	gdk_threads_enter();
	{

	cairo_t *dragging_dc;
	// clean last step from dragging background - [added since we now paint to draggin_layer]
		dragging_dc = cairo_create(dragging_background);
		cairo_rectangle(dragging_dc, (piece->colour?floor(prev_x1-wi/16.0f):floor(prev_x2-wi/16.0f)), (piece->colour?floor(prev_y1-hi/16.0f):floor(prev_y2-hi/16.0f)), ceil(ww), ceil(hh));
		cairo_clip(dragging_dc);
		paint_layers(dragging_dc);
		cairo_destroy(dragging_dc);

		// paint piece on it - [added since we now paint to dragging_layer]
		dragging_dc = cairo_create(dragging_background);
		cairo_set_source_surface(dragging_dc, piece->surf, xx-wi/16, yy-hi/16);
		cairo_rectangle(dragging_dc, floor(xx-wi/16), floor(yy-hi/16), ceil(ww), ceil(hh));
		cairo_clip(dragging_dc);
		cairo_paint(dragging_dc);
		cairo_destroy(dragging_dc);


		// paint buffer surface with dragging background
		cairo_t *cache_dc = cairo_create(cache_layer);
		cairo_set_source_surface (cache_dc, dragging_background, 0.0f, 0.0f);
		cairo_rectangle(dragging_dc, (piece->colour?floor(prev_x1-wi/16.0f):floor(prev_x2-wi/16.0f)), (piece->colour?floor(prev_y1-hi/16.0f):floor(prev_y2-hi/16.0f)), ceil(ww), ceil(hh));
		cairo_rectangle(cache_dc, floor(xx-wi/16), floor(yy-hi/16), ceil(ww), ceil(hh));
		cairo_clip(cache_dc);
		cairo_paint(cache_dc);

		// paint animated piece at new position - [removed since we now paint to dragging_layer]
	//	cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
	//	cairo_set_source_surface (cache_dc, anim->piece->surf, xx-wi/16.0f, yy-hi/16.0f);
	//	cairo_paint(cache_dc);

		// If a piece is being dragged and overlaps with the animation, repaint the dragged piece above to cache layer
		if (mouse_dragged_piece != NULL && g_atomic_int_get(&moveit_flag)) {
			int dragged_x = g_atomic_int_get(&dragging_prev_x);
			int dragged_y = g_atomic_int_get(&dragging_prev_y);
			//FIXME: better lock access to mouse_dragged_piece (this could segfault otherwise)
			cairo_set_source_surface (cache_dc, mouse_dragged_piece->surf, dragged_x-wi/16.0f, dragged_y-hi/16.0f);
			cairo_set_operator(cache_dc, CAIRO_OPERATOR_OVER);
			cairo_paint(cache_dc);
		}

		cairo_destroy(cache_dc);



		// If the board isn't drawable we're probably exiting
		// free up allocated memory and stop animation
		if (!GDK_IS_DRAWABLE(board->window)) {
			debug("Aborting animation.\n");
			gdk_threads_leave();
			return FALSE;
		}
		cairo_t *cdr = gdk_cairo_create (board->window);
		cairo_rectangle(cdr, floor(xx-wi/16.0f), floor(yy-hi/16.0f), ceil(ww), ceil(hh));
		cairo_rectangle(cdr, (piece->colour?floor(prev_x1-wi/16.0f):floor(prev_x2-wi/16.0f)), (piece->colour?floor(prev_y1-hi/16.0f):floor(prev_y2-hi/16.0f)), ceil(ww), ceil(hh));
		cairo_clip(cdr);

		// apply buffered surface to cr (NB: cr is clipped)
		cairo_set_operator (cdr, CAIRO_OPERATOR_OVER);
		cairo_set_source_surface (cdr, cache_layer, 0.0f, 0.0f);
		cairo_paint(cdr);

		// debug
		//cairo_set_source_rgba (cdr, 1, 0, 0, .5f);
		//cairo_paint(cdr);

		cairo_destroy(cdr);
	}
	/* LEAVE THREADS */
	gdk_threads_leave();

	if (piece->colour) {
		prev_x1 = xx;
		prev_y1 = yy;
	}
	else {
		prev_x2 = xx;
		prev_y2 = yy;
	}


	return TRUE;
}

