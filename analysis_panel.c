#include <gtk/gtk.h>

#include "analysis_panel.h"

static GtkWidget* score_label;
static GtkWidget* line_label;
static GtkWidget* nps_label;

GtkWidget *create_analysis_panel(void) {
	score_label = gtk_label_new("0");
	line_label = gtk_label_new("");
	nps_label = gtk_label_new("0 kNps");

	gtk_label_set_xalign(GTK_LABEL(score_label), 0);
	gtk_label_set_xalign(GTK_LABEL(line_label), 0);
	gtk_label_set_xalign(GTK_LABEL(nps_label), 0);

	gtk_label_set_max_width_chars(GTK_LABEL(line_label), 1);
	gtk_label_set_ellipsize(GTK_LABEL(line_label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand(GTK_WIDGET(line_label), TRUE);

	GtkWidget *wrapper_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	GtkStyleContext *context = gtk_widget_get_style_context(wrapper_box);
	gtk_style_context_add_class(context,"analysis-panel-contents");

	gtk_box_pack_start(GTK_BOX(wrapper_box), score_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(wrapper_box), line_label, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(wrapper_box), nps_label, FALSE, FALSE, 0);
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

void set_analysis_nodes_per_second(const char *nps) {
	gdk_threads_enter();
	gtk_label_set_text(GTK_LABEL(nps_label), nps);
	gdk_threads_leave();
}