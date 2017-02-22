#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <string.h>

#include "clock-widget.h"

static const char *FONT_FACE = "DSEG7 Classic Bold";
extern GtkWidget *board;
static pthread_mutex_t mutex_drawing = PTHREAD_MUTEX_INITIALIZER;

G_DEFINE_TYPE (ClockFace, clock_face, GTK_TYPE_DRAWING_AREA) ;

// Colours
static double active_bg_r = 80 / 255.0;
static double active_bg_g = 90 / 255.0;
static double active_bg_b = 96 / 255.0;
static double inactive_bg_r = 38 / 255.0;
static double inactive_bg_g = 38 / 255.0;
static double inactive_bg_b = 38 / 255.0;

static char active_fg_hex[] = "#FFFFFF";
static double active_fg_r = 1.0;
static double active_fg_g = 1.0;
static double active_fg_b = 1.0;

static char inactive_fg_hex[] = "#929292";
static double inactive_fg_r = 146 / 255.0;
static double inactive_fg_g = 146 / 255.0;
static double inactive_fg_b = 146 / 255.0;

static double warn_bg_r = 160 / 255.0;
static double warn_bg_g = 0 / 255.0;
static double warn_bg_b = 0 / 255.0;

static char warn_fg_ghost_hex[] = "#B41414";
static double warn_fg_ghost_r = 180 / 255.0;
static double warn_fg_ghost_g = 20 / 255.0;
static double warn_fg_ghost_b = 20 / 255.0;

static char active_fg_ghost_hex[] = "#5F696F";
static double active_fg_ghost_r = 95 / 255.0;
static double active_fg_ghost_g = 105 / 255.0;
static double active_fg_ghost_b = 111 / 255.0;

static char inactive_fg_ghost_hex[] = "#353535";
static double inactive_fg_ghost_r = 53 / 255.0;
static double inactive_fg_ghost_g = 53 / 255.0;
static double inactive_fg_ghost_b = 53 / 255.0;

void draw_clock_face(GtkWidget *clock_face, cairo_t *crt) {

	ClockFace *cf = CLOCK_FACE(clock_face);

	if (!cf->clock) {
		fprintf(stderr, "Can't draw a NULL clock\n");
		return;
	}

	pthread_mutex_lock( &mutex_drawing );

	cairo_surface_t *buffer_surf = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, 
			clock_face->allocation.width, 
			clock_face->allocation.height);

	cairo_t *buff_crt = cairo_create(buffer_surf);

	char white[32];
	char white_ghost[32];
	char black[32];
	char black_ghost[32];

	clock_to_string(cf->clock, 0, white, white_ghost);
	clock_to_string(cf->clock, 1, black, black_ghost);

	PangoFontDescription *desc;
	PangoLayout *layout;

	layout = pango_cairo_create_layout(buff_crt);

	char font_str[32];

	float hi_font_size;
	float wi_font_size;
	float font_size;
	int wi = clock_face->allocation.width;
	int hi = clock_face->allocation.height;

	// Create and Set font description

	/* *
	 * Find Optimal Font Size in points
	 * the initial guessed size is an average from experiments 
	 * but is adjusted for the current display if too big
	 * */
	double halfWidth = (double) wi / 2.0f;
	hi_font_size = (float) (hi / 1.58);
	wi_font_size = (float) (wi / 17.45);
	font_size = (hi_font_size < wi_font_size) ? hi_font_size : wi_font_size;

	sprintf(font_str, "%s %.1f", FONT_FACE, font_size);
	desc = pango_font_description_from_string(font_str);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	// Check that both clocks fit
	int pix_width, pix_height;
	pango_layout_set_text(layout, black, -1);
	pango_layout_get_pixel_size(layout, &pix_width, &pix_height);
	while ((pix_height > hi - 10 || pix_width > halfWidth - 20) && font_size > 0) {
		font_size -= .1;
		sprintf(font_str, "%s %.2f", FONT_FACE, font_size);
		desc = pango_font_description_from_string(font_str);
		pango_layout_set_font_description(layout, desc);
		pango_font_description_free(desc);
		pango_layout_get_pixel_size(layout, &pix_width, &pix_height);
	}
	pango_layout_set_text(layout, white, -1);
	pango_layout_get_pixel_size(layout, &pix_width, &pix_height);
	while ((pix_height > hi - 10 || pix_width > halfWidth - 20) && font_size > 0) {
		font_size -= .1;
		sprintf(font_str, "%s %.2f", FONT_FACE, font_size);
		desc = pango_font_description_from_string(font_str);
		pango_layout_set_font_description(layout, desc);
		pango_font_description_free(desc);
		pango_layout_get_pixel_size(layout, &pix_width, &pix_height);
	}

	int wa = is_active(cf->clock, 0);
	int ba = is_active(cf->clock, 1);
	int my_color = -1;
	int warn_me = 0;

	if (cf->clock->relation > 0) {
		my_color = 0;
	}
	else if (cf->clock->relation < 0) {
		my_color = 1;
	}

	if (my_color > -1) {
		if (my_color ? ba : wa) {
			warn_me = am_low_on_time(cf->clock);
		}
	}

	cairo_save(buff_crt);

	struct timeval my_time = cf->clock->remaining_time[cf->clock->relation > 0 ? 0 : 1];
	struct timeval active_time = cf->clock->remaining_time[wa ? 0 : 1];
	bool warn_toggle;
	bool colon_toggle;
	if (my_time.tv_sec <= 0) {
		warn_toggle = true;
		colon_toggle = true;
	} else {
		warn_toggle = my_time.tv_usec > 500000;
		colon_toggle = active_time.tv_usec > 500000;
	}

	// paint white's clock background
	if (wa) {
		if (warn_me && warn_toggle) {
			cairo_set_source_rgb(buff_crt, warn_bg_r, warn_bg_g, warn_bg_b);
		} else {
			cairo_set_source_rgb(buff_crt, active_bg_r, active_bg_g, active_bg_b);
		}
	} else {
		cairo_set_source_rgb(buff_crt, inactive_bg_r, inactive_bg_g, inactive_bg_b);
	}
	cairo_rectangle(buff_crt, 0, 0, halfWidth, hi);
	cairo_fill(buff_crt);

	// paint white's clock text
	double tx = .5 * (wi / 2.0f - pix_width);
	double ty = .5 * (hi - pix_height);
	cairo_translate(buff_crt, tx, ty);

	pango_layout_set_text(layout, white_ghost, -1);
	char *colon_colour;
	if (wa) {
		colon_colour = colon_toggle ? active_fg_hex : active_fg_ghost_hex;
		if (warn_me && warn_toggle) {
			cairo_set_source_rgb(buff_crt, warn_fg_ghost_r, warn_fg_ghost_g, warn_fg_ghost_b);
			colon_colour = colon_toggle ? active_fg_hex : warn_fg_ghost_hex;
		} else {
			cairo_set_source_rgb(buff_crt, active_fg_ghost_r, active_fg_ghost_g, active_fg_ghost_b);
		}
	} else {
		colon_colour = inactive_fg_hex;
		cairo_set_source_rgb(buff_crt, inactive_fg_ghost_r, inactive_fg_ghost_g, inactive_fg_ghost_b);
	}
	pango_cairo_show_layout(buff_crt, layout);

	char markup[256];
	char *colon = strchr(white, ':');
	char before_colon[16];
	char after_colon[16];
	memset(before_colon, 0, 16);
	memset(after_colon, 0, 16);
	memcpy(before_colon, white, colon - white);
	size_t prefixLen = strlen(before_colon);
	size_t whiteLen = strlen(white);
	memcpy(after_colon, colon + 1, whiteLen - prefixLen - 1);

	sprintf(markup, "%s<span foreground=\"%s\">:</span>%s", before_colon, colon_colour, after_colon);
	pango_layout_set_markup(layout, markup, -1);
	if (wa) {
		cairo_set_source_rgb(buff_crt, active_fg_r, active_fg_g, active_fg_b);
	} else {
		cairo_set_source_rgb(buff_crt, inactive_fg_r, inactive_fg_g, inactive_fg_b);
	}
	pango_cairo_show_layout(buff_crt, layout);


	cairo_restore(buff_crt);
	cairo_save(buff_crt);

	// paint black's clock background
	cairo_translate(buff_crt, halfWidth, 0);

	if (ba) {
		if (warn_me && warn_toggle) {
			cairo_set_source_rgb(buff_crt, warn_bg_r, warn_bg_g, warn_bg_b);
		} else {
			cairo_set_source_rgb(buff_crt, active_bg_r, active_bg_g, active_bg_b);
		}
	} else {
		cairo_set_source_rgb(buff_crt, inactive_bg_r, inactive_bg_g, inactive_bg_b);
	}
	cairo_rectangle(buff_crt, 0, 0, halfWidth, hi);
	cairo_fill(buff_crt);

	// paint black's clock text
    tx = .5 * (wi / 2.0f - pix_width);
    cairo_translate(buff_crt, tx, ty);
    pango_layout_set_text(layout, black_ghost, -1);
    if (ba) {
        colon_colour = colon_toggle ? active_fg_hex : active_fg_ghost_hex;
        if (warn_me && warn_toggle) {
            cairo_set_source_rgb(buff_crt, warn_fg_ghost_r, warn_fg_ghost_g, warn_fg_ghost_b);
            colon_colour = colon_toggle ? active_fg_hex : warn_fg_ghost_hex;
        } else {
            cairo_set_source_rgb(buff_crt, active_fg_ghost_r, active_fg_ghost_g, active_fg_ghost_b);
        }
    } else {
        colon_colour = inactive_fg_hex;
        cairo_set_source_rgb(buff_crt, inactive_fg_ghost_r, inactive_fg_ghost_g, inactive_fg_ghost_b);
    }
    pango_cairo_show_layout(buff_crt, layout);

	colon = strchr(black, ':');
	memset(before_colon, 0, 16);
	memset(after_colon, 0, 16);
	memcpy(before_colon, black, colon - black);
	prefixLen = strlen(before_colon);
	whiteLen = strlen(black);
	memcpy(after_colon, colon + 1, whiteLen - prefixLen - 1);

	sprintf(markup, "%s<span foreground=\"%s\">:</span>%s", before_colon, colon_colour, after_colon);
	pango_layout_set_markup(layout, markup, -1);
	pango_layout_get_pixel_size(layout, &pix_width, &pix_height);

	if (ba) {
		if (warn_me && warn_toggle) {
			cairo_set_source_rgb(buff_crt, warn_fg_ghost_r, warn_fg_ghost_g, warn_fg_ghost_b);
		} else {
			cairo_set_source_rgb(buff_crt, active_fg_ghost_r, active_fg_ghost_g, active_fg_ghost_b);
		}
	} else {
		cairo_set_source_rgb(buff_crt, inactive_fg_ghost_r, inactive_fg_ghost_g, inactive_fg_ghost_b);
	}
	pango_cairo_show_layout(buff_crt, layout);

	if (ba) {
		cairo_set_source_rgb(buff_crt, active_fg_r, active_fg_g, active_fg_b);
	}
	else {
		cairo_set_source_rgb(buff_crt, inactive_fg_r, inactive_fg_g, inactive_fg_b);
	}
	pango_layout_set_text(layout, black, -1);
	pango_cairo_show_layout(buff_crt, layout);

	// debug text boxes
	cairo_restore(buff_crt);
//	pango_layout_set_text(layout, white, -1);
//	pango_layout_get_pixel_size(layout, &pix_width, &pix_height);
//	double rx = .5 * (wi / 2.0f - pix_width);
//	double ry = .5 * (hi - pix_height);
//	cairo_set_source_rgb(buff_crt, 1, 1, 1);
//	cairo_rectangle(buff_crt, rx, ry, pix_width, pix_height);
//	cairo_set_line_width(buff_crt, 1);
//	cairo_stroke(buff_crt);

	g_object_unref(layout);

	// paint separator on boundary to avoid aliasing
	if (wa || ba) {
		cairo_set_source_rgb(buff_crt, (active_bg_r + inactive_bg_r) / 2.0, (active_bg_g + inactive_bg_g) / 2.0, (active_bg_b + inactive_bg_b) / 2.0);
	} else {
		cairo_set_source_rgb(buff_crt, inactive_bg_r, inactive_bg_g, inactive_bg_b);
	}
	cairo_move_to (buff_crt, halfWidth, 0);
	cairo_line_to(buff_crt, halfWidth, hi);
	cairo_set_line_width(buff_crt, 1.0f);
	cairo_stroke(buff_crt);

	cairo_destroy(buff_crt);

	// Apply cache surface to crt
	cairo_set_source_surface(crt, buffer_surf, 0.0f, 0.0f);
	cairo_paint(crt);

	cairo_surface_destroy(buffer_surf);

	pthread_mutex_unlock(&mutex_drawing);
}

static gboolean clock_face_expose(GtkWidget *clock_face, GdkEventExpose *event) {

	cairo_t *cr;

	/* get a cairo_t */
	cr = gdk_cairo_create (clock_face->window);

	/* set a clip region for the expose event */
	cairo_rectangle (cr, event->area.x, event->area.y, event->area.width, event->area.height);
	cairo_clip (cr);
	draw_clock_face(clock_face, cr);
	cairo_destroy (cr);

	return FALSE;
}

static void clock_face_class_init (ClockFaceClass *class) {
	GtkWidgetClass *widget_class;
	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->expose_event = clock_face_expose;
	//widget_class->configure_event = clock_face_configure;
	//widget_class->size_request = clock_face_size_request;
	//widget_class->size_allocate = clock_face_size_allocate;
}


static void clock_face_init(ClockFace *clock_face) {
}

void clock_face_set_clock(ClockFace *clock_face, chess_clock *clock) {
	clock_face->clock = clock;
	if (clock) {
		clock->parent = GTK_WIDGET(clock_face);
	}
}

GtkWidget *clock_face_new(void) {
	return g_object_new(TYPE_CLOCK_FACE, NULL);
}

gboolean periodic_refresh (gpointer data) {
	if (!GTK_IS_WIDGET(data)) {
		// Killed? tough
		//printf("refresher: killed?\n");
		return FALSE;
	}
	gtk_widget_queue_draw(GTK_WIDGET(data));
	return TRUE;
}

void refresh_both_clocks (GtkWidget *clock) {
	if (!GTK_IS_WIDGET(clock)) {
		// Killed? tough
		return;
	}
	gtk_widget_queue_draw(clock);
}

void refresh_one_clock(GtkWidget *clock, int black) {
	if (!GTK_WIDGET(clock)) {
		return;
	}
	int half_w = clock->allocation.width / 2;
	gtk_widget_queue_draw_area(clock, black? half_w : 0, 0, half_w, clock->allocation.height);
}


/*int a_main(int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *clock;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	clock = clock_face_new();
	init_clock(&myclock, 65, 0, 0);
	clock_face_set_clock( CLOCK_FACE(clock), &myclock);

	gtk_container_add(GTK_CONTAINER (window), clock);

	g_signal_connect(window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
	gtk_widget_add_events (window, GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK);
	g_signal_connect (G_OBJECT (window), "button-press-event", G_CALLBACK (on_press), NULL);

	start_one_clock(&myclock, 0);
	g_timeout_add(100, periodic_refresh, clock);

	gtk_window_set_default_size(GTK_WINDOW(window), 500, 100);
	gtk_widget_show_all (window);
	gtk_main ();

	return 0;
}
*/

