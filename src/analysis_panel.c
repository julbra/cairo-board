#include <gtk/gtk.h>

#include "analysis_panel.h"

static GtkWidget* score_label;
static GtkWidget* line_label;
static GtkWidget* depth_label;
static GtkWidget* nps_label;
static GtkWidget* engine_name_label;

void add_class(GtkWidget *widget, const char* class) {
	GtkStyleContext *context = gtk_widget_get_style_context(widget);
	gtk_style_context_add_class(context, class);
}

GtkWidget *create_analysis_panel(void) {
	score_label = gtk_label_new("0");
	add_class(score_label, "score-label");
	gtk_widget_set_size_request(score_label, 150, -1);
	gtk_label_set_xalign(GTK_LABEL(score_label), 0.5);
	gtk_widget_set_hexpand(GTK_WIDGET(score_label), FALSE);

	depth_label = gtk_label_new("");
	add_class(depth_label, "depth-label");
	gtk_label_set_xalign(GTK_LABEL(depth_label), 0);
	gtk_widget_set_size_request(depth_label, 100, -1);

	nps_label = gtk_label_new("0 kN/s");
	add_class(nps_label, "nps-label");
	gtk_label_set_xalign(GTK_LABEL(nps_label), 0);

	engine_name_label = gtk_label_new("Stockfish 8");
	add_class(engine_name_label, "engine-name-label");
	gtk_label_set_xalign(GTK_LABEL(engine_name_label), 0);

	line_label = gtk_label_new("");
	add_class(line_label, "best-line-label");
	gtk_label_set_xalign(GTK_LABEL(line_label), 0);
	gtk_label_set_max_width_chars(GTK_LABEL(line_label), 1);
	gtk_label_set_ellipsize(GTK_LABEL(line_label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand(GTK_WIDGET(line_label), TRUE);

	GtkWidget *depth_speed_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	add_class(depth_speed_box, "depth-speed-box");
	gtk_box_pack_start(GTK_BOX(depth_speed_box), depth_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(depth_speed_box), nps_label, TRUE, TRUE, 0);

	GtkWidget *details_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	add_class(details_box, "details-box");
	gtk_box_pack_start(GTK_BOX(details_box), engine_name_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(details_box), depth_speed_box, TRUE, TRUE, 0);

	GtkWidget *top_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	add_class(top_box, "top-box");
	gtk_box_pack_start(GTK_BOX(top_box), score_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(top_box), details_box, TRUE, TRUE, 0);

	GtkWidget *wrapper_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	add_class(wrapper_box, "analysis-panel-contents");
	gtk_box_pack_start(GTK_BOX(wrapper_box), top_box, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(wrapper_box), line_label, TRUE, TRUE, 0);

	return wrapper_box;
}

void set_analysis_score(const char *score_value) {
	gdk_threads_enter();
	gtk_label_set_text(GTK_LABEL(score_label), score_value);
	gdk_threads_leave();
}

void set_analysis_best_line(const char *best_line) {
	gdk_threads_enter();
	gtk_label_set_text(GTK_LABEL(line_label), best_line);
	gdk_threads_leave();
}

void set_analysis_depth(const char *depth) {
	gdk_threads_enter();
	gtk_label_set_text(GTK_LABEL(depth_label), depth);
	gdk_threads_leave();
}

void set_analysis_nodes_per_second(const char *nps) {
	gdk_threads_enter();
	gtk_label_set_text(GTK_LABEL(nps_label), nps);
	gdk_threads_leave();
}

void set_analysis_engine_name(const char *engine_name) {
	gdk_threads_enter();
	gtk_label_set_text(GTK_LABEL(engine_name_label), engine_name);
	gdk_threads_leave();
}