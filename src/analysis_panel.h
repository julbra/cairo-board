#ifndef CAIRO_BOARD_ANALYSIS_PANEL_H
#define CAIRO_BOARD_ANALYSIS_PANEL_H

GtkWidget *create_analysis_panel(void);

void set_analysis_score(const char *);

void set_analysis_best_line(const char *);

void set_analysis_depth(const char *);

void set_analysis_nodes_per_second(const char *);

void set_analysis_engine_name(const char *);

#endif //CAIRO_BOARD_ANALYSIS_PANEL_H
