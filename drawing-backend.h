/*
 * drawing-backend.h
 *
 *  Created on: 25 Nov 2009
 *      Author: hts
 */

#ifndef DRAWINGBACKEND_H_
#define DRAWINGBACKEND_H_

#include "cairo-board.h"

#define ANIM_SIZE 2048

void *process_moves(void *ptr);
void reset_board(void);
void expose_update(cairo_t *cdr, double wi, double hi);
void expose_scale(cairo_t *cdr, double wi, double hi);
void expose_clip(cairo_t *cdr, GdkEventExpose* event, double wi, double hi);
void handle_button_release(void);
void handle_left_button_press(GtkWidget *pWidget, int wi, int hi, int x, int y);
void handle_right_button_press(GtkWidget *pWidget, int wi, int hi);
void handle_middle_button_press(GtkWidget *pWidget, int wi, int hi);
void handle_flip_board(GtkWidget *pWidget);
void init_dragging_background(int wi, int hi);
void init_highlight_surface(int wi, int hi);

//gboolean choose_promote(int value, gboolean only_surfaces);
void choose_promote(int last_promote, gboolean only_surfaces, int ocol, int orow, int ncol, int nrow, gboolean lock_threads);
void choose_promote_handler(void *GtkWidget, gpointer value);
void choose_promote_deactivate_handler(void *GtkWidget, gpointer value, gboolean only_surfaces);

void init_anims_map(void);

// TEST
gboolean test_animate_random_step(gpointer data);

extern int lm_source_col;

#endif /* DRAWINGBACKEND_H_ */
