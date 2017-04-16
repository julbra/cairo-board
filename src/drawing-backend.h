/*
 * drawing-backend.h
 *
 *  Created on: 25 Nov 2009
 *      Author: hts
 */

#ifndef DRAWINGBACKEND_H_
#define DRAWINGBACKEND_H_

#include "cairo-board.h"
#include "chess-backend.h"

#define ANIM_SIZE 2048

void *process_moves(void *ptr);
void reset_board(void);
void draw_full_update(cairo_t *cdr, int wi, int hi);
void draw_scaled(cairo_t *cdr, int wi, int hi);
void draw_cheap_repaint(cairo_t *cdr, int wi, int hi);
void handle_left_mouse_up(void);
void handle_left_mouse_down(GtkWidget *pWidget, int wi, int hi, int x, int y);
void handle_right_button_press(GtkWidget *pWidget, int wi, int hi);
void handle_middle_button_press(GtkWidget *pWidget, int wi, int hi);
void handle_flip_board(GtkWidget *pWidget, bool lock_threads);
void init_dragging_background(int wi, int hi);
void init_highlight_under_surface(int wi, int hi);
void init_highlight_over_surface(int wi, int hi);
void draw_board_surface(int wi, int hi);
void draw_pieces_surface(int wi, int hi);
void highlight_move(int source_col, int source_row, int dest_col, int dest_row, int wi, int hi);
void highlight_pre_move(int pre_move[4], int wi, int hi);
void cancel_pre_move(int wi, int hi, bool lock_threads);
void warn_check(int wi, int hi);

void choose_promote(int last_promote, bool only_surfaces, bool only_logical, int ocol, int orow, int ncol, int nrow);
void choose_promote_handler(void *GtkWidget, gpointer value);
void choose_promote_deactivate_handler(void *GtkWidget, gpointer value, gboolean only_surfaces);

void init_anims_map(void);

// TEST
gboolean test_animate_random_step(gpointer data);

#endif /* DRAWINGBACKEND_H_ */
