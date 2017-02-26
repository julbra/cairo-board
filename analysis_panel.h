#ifndef CAIRO_BOARD_ANALYSIS_PANEL_H
#define CAIRO_BOARD_ANALYSIS_PANEL_H

GtkWidget *create_analysis_panel(void);

void set_analysis_score(const char *score_value);

void set_analysis_best_line(const char *best_line);

void set_analysis_depth(const char *depth);

void set_analysis_nodes_per_second(const char *nps);

#endif //CAIRO_BOARD_ANALYSIS_PANEL_H
