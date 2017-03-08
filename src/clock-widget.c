#include <gtk/gtk.h>
#include <math.h>
#include <string.h>

#include "clock-widget.h"

static const char *FONT_FACE = "DSEG7 Classic Bold";
extern GtkWidget *board;
static pthread_mutex_t mutex_drawing = PTHREAD_MUTEX_INITIALIZER;

G_DEFINE_TYPE (ClockFace, clock_face, GTK_TYPE_DRAWING_AREA) ;

// Colours
enum {
	// Active clock background
	ACTIVE_BG_R = 80,
	ACTIVE_BG_G = 90,
	ACTIVE_BG_B = 96,

	// Inactive clock background
	INACTIVE_BG_R = 38,
	INACTIVE_BG_G = 38,
	INACTIVE_BG_B = 38,

	// Active clock font colour
	ACTIVE_FG_R = 255,
	ACTIVE_FG_G = 255,
	ACTIVE_FG_B = 255,

	// Inactive clock font colour
	INACTIVE_FG_R = 146,
	INACTIVE_FG_G = 146,
	INACTIVE_FG_B = 146,

	// Low-on-time warning background
	WARN_BG_R = 160,
	WARN_BG_G = 0,
	WARN_BG_B = 0,

	// Active clock font colour for 7 segment ghost effect
	ACTIVE_FG_GHOST_R = 93,
	ACTIVE_FG_GHOST_G = 103,
	ACTIVE_FG_GHOST_B = 109,

	// Inactive clock font colour for 7 segment ghost effect
	INACTIVE_FG_GHOST_R = 51,
	INACTIVE_FG_GHOST_G = 51,
	INACTIVE_FG_GHOST_B = 51,

	// Low-on-time warning font colour for 7 segment ghost effect
	WARN_FG_GHOST_R = 180,
	WARN_FG_GHOST_G = 20,
	WARN_FG_GHOST_B = 20
};

// Active clock background
static double active_bg_r = ACTIVE_BG_R / 255.0;
static double active_bg_g = ACTIVE_BG_G / 255.0;
static double active_bg_b = ACTIVE_BG_B / 255.0;

// Inactive clock background
static double inactive_bg_r = INACTIVE_BG_R / 255.0;
static double inactive_bg_g = INACTIVE_BG_G / 255.0;
static double inactive_bg_b = INACTIVE_BG_B / 255.0;

// Active clock font colour
static char active_fg_hex[8];
static double active_fg_r = ACTIVE_FG_R / 255.0;
static double active_fg_g = ACTIVE_FG_G / 255.0;
static double active_fg_b = ACTIVE_FG_B / 255.0;

// Inactive clock font colour
static char inactive_fg_hex[8];
static double inactive_fg_r = INACTIVE_FG_R / 255.0;
static double inactive_fg_g = INACTIVE_FG_G / 255.0;
static double inactive_fg_b = INACTIVE_FG_B / 255.0;

// Low-on-time warning background
static double warn_bg_r = WARN_BG_R / 255.0;
static double warn_bg_g = WARN_BG_G / 255.0;
static double warn_bg_b = WARN_BG_B / 255.0;

// Active clock font colour for 7 segment ghost effect
static char active_fg_ghost_hex[8];
static double active_fg_ghost_r = ACTIVE_FG_GHOST_R / 255.0;
static double active_fg_ghost_g = ACTIVE_FG_GHOST_G / 255.0;
static double active_fg_ghost_b = ACTIVE_FG_GHOST_B / 255.0;

// Inactive clock font colour for 7 segment ghost effect
static char inactive_fg_ghost_hex[8];
static double inactive_fg_ghost_r = INACTIVE_FG_GHOST_R / 255.0;
static double inactive_fg_ghost_g = INACTIVE_FG_GHOST_G / 255.0;
static double inactive_fg_ghost_b = INACTIVE_FG_GHOST_B / 255.0;

// Low-on-time warning font colour for 7 segment ghost effect
static char warn_fg_ghost_hex[8];
static double warn_fg_ghost_r = WARN_FG_GHOST_R / 255.0;
static double warn_fg_ghost_g = WARN_FG_GHOST_G / 255.0;
static double warn_fg_ghost_b = WARN_FG_GHOST_B / 255.0;

void rgb_to_css(char *hex, int r, int g, int b) {
	sprintf(hex, "#%02X%02X%02X", r & 0xFF, g & 0xFF, b & 0xFF);
}

void init_clock_colours(void) {
	rgb_to_css(active_fg_hex, ACTIVE_FG_R, ACTIVE_FG_G, ACTIVE_FG_B);
	rgb_to_css(inactive_fg_hex, INACTIVE_FG_R, INACTIVE_FG_G, INACTIVE_FG_B);
	rgb_to_css(active_fg_ghost_hex, ACTIVE_FG_GHOST_R, ACTIVE_FG_GHOST_G, ACTIVE_FG_GHOST_B);
	rgb_to_css(inactive_fg_ghost_hex, INACTIVE_FG_GHOST_R, INACTIVE_FG_GHOST_G, INACTIVE_FG_GHOST_B);
	rgb_to_css(warn_fg_ghost_hex, WARN_FG_GHOST_R, WARN_FG_GHOST_G, WARN_FG_GHOST_B);
}

gboolean draw_clock_face(GtkWidget *clock_face, cairo_t *crt) {

	static int last_wi = -1;
	static int last_hi = -1;
	static size_t last_white_len = 0;
	static size_t last_black_len = 0;
	static float font_size = -1;

	ClockFace *cf = CLOCK_FACE(clock_face);

	if (!cf->clock) {
		fprintf(stderr, "Can't draw a NULL clock\n");
		return FALSE;
	}

	pthread_mutex_lock(&mutex_drawing);

	cairo_surface_t *buffer_surf = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, 
			gtk_widget_get_allocated_width(clock_face),
			gtk_widget_get_allocated_height(clock_face));

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

	int wi = gtk_widget_get_allocated_width(clock_face);
	int hi = gtk_widget_get_allocated_height(clock_face);

	size_t white_len = strlen(white);
	size_t black_len = strlen(black);

	// Create and Set font description

	/* *
	 * Find Optimal Font Size in points
	 * the initial guessed size is an average from experiments 
	 * but is adjusted for the current display if too big
	 * */
	double halfWidth = (double) wi / 2.0f;
	int w_pix_width, w_pix_height, b_pix_width, b_pix_height;

	if (last_wi == wi && last_hi == hi && last_white_len == white_len && last_black_len == black_len) {
		// Reuse font size as previously calculated
		sprintf(font_str, "%s %.1f", FONT_FACE, font_size);
		desc = pango_font_description_from_string(font_str);
		pango_layout_set_font_description(layout, desc);
		pango_font_description_free(desc);

		pango_layout_set_text(layout, black, -1);
		pango_layout_get_pixel_size(layout, &b_pix_width, &b_pix_height);

		pango_layout_set_text(layout, white, -1);
		pango_layout_get_pixel_size(layout, &w_pix_width, &w_pix_height);
	} else {
		// Calculating new font size since things have changed
		float last_font_size = font_size;

		int v_padding = 10;
		int h_padding = 20;

		hi_font_size = (float) (hi / 1.5);
		wi_font_size = (float) (wi / 9.0);
		font_size = (hi_font_size < wi_font_size) ? hi_font_size : wi_font_size;

		sprintf(font_str, "%s %.1f", FONT_FACE, font_size);
		desc = pango_font_description_from_string(font_str);
		pango_layout_set_font_description(layout, desc);
		pango_font_description_free(desc);

		// Ensure both clocks fit
		pango_layout_set_text(layout, black, -1);
		pango_layout_get_pixel_size(layout, &b_pix_width, &b_pix_height);
		while ((b_pix_height > hi - v_padding || b_pix_width > halfWidth - h_padding) && font_size > 1) {
			font_size -= .1;
			sprintf(font_str, "%s %.2f", FONT_FACE, font_size);
			desc = pango_font_description_from_string(font_str);
			pango_layout_set_font_description(layout, desc);
			pango_font_description_free(desc);
			pango_layout_get_pixel_size(layout, &b_pix_width, &b_pix_height);
		}
		pango_layout_set_text(layout, white, -1);
		pango_layout_get_pixel_size(layout, &w_pix_width, &w_pix_height);
		while ((w_pix_height > hi - v_padding || w_pix_width > halfWidth - h_padding) && font_size > 1) {
			font_size -= .1;
			sprintf(font_str, "%s %.2f", FONT_FACE, font_size);
			desc = pango_font_description_from_string(font_str);
			pango_layout_set_font_description(layout, desc);
			pango_font_description_free(desc);
			pango_layout_get_pixel_size(layout, &w_pix_width, &w_pix_height);
		}

		if (last_font_size > -1 && font_size != last_font_size) {
			// Redraw whole clock as font size of both should change
			gtk_widget_queue_draw(clock_face);
		}
	}

	last_wi = wi;
	last_hi = hi;
	last_white_len = white_len;
	last_black_len = black_len;

//	printf("Allocated height == %dpx\n", hi);
//	printf("Final font size %f\n", font_size);
//	printf("Final H ratio %f\n", hi / font_size);
//	printf("Final W ratio %f\n", wi / font_size);

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
	double w_bg_r, w_bg_g, w_bg_b;
	if (wa) {
		if (warn_me && warn_toggle) {
			w_bg_r = warn_bg_r;
			w_bg_g = warn_bg_g;
			w_bg_b = warn_bg_b;
		} else {
			w_bg_r = active_bg_r;
			w_bg_g = active_bg_g;
			w_bg_b = active_bg_b;
		}
	} else {
		w_bg_r = inactive_bg_r;
		w_bg_g = inactive_bg_g;
		w_bg_b = inactive_bg_b;
	}
	cairo_set_source_rgb(buff_crt, w_bg_r, w_bg_g, w_bg_b);
	cairo_rectangle(buff_crt, 0, 0, halfWidth, hi);
	cairo_fill(buff_crt);

	// paint white's clock text
	double tx = .5 * (wi / 2.0f - w_pix_width);
	double ty = .5 * (hi - w_pix_height);
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
	char *colon = strrchr(white, ':');
	char before_colon[16];
	char after_colon[16];
	memset(before_colon, 0, 16);
	memset(after_colon, 0, 16);
	memcpy(before_colon, white, colon - white);
	size_t prefixLen = strlen(before_colon);
	memcpy(after_colon, colon + 1, white_len - prefixLen - 1);

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

	double b_bg_r, b_bg_g, b_bg_b;
	if (ba) {
		if (warn_me && warn_toggle) {
			b_bg_r = warn_bg_r;
			b_bg_g = warn_bg_g;
			b_bg_b = warn_bg_b;
		} else {
			b_bg_r = active_bg_r;
			b_bg_g = active_bg_g;
			b_bg_b = active_bg_b;
		}
	} else {
		b_bg_r = inactive_bg_r;
		b_bg_g = inactive_bg_g;
		b_bg_b = inactive_bg_b;
	}
	cairo_set_source_rgb(buff_crt, b_bg_r, b_bg_g, b_bg_b);
	cairo_rectangle(buff_crt, 0, 0, halfWidth, hi);
	cairo_fill(buff_crt);

	// paint black's clock text
	tx = .5 * (wi / 2.0f - b_pix_width);
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

	colon = strrchr(black, ':');
	memset(before_colon, 0, 16);
	memset(after_colon, 0, 16);
	memcpy(before_colon, black, colon - black);
	prefixLen = strlen(before_colon);
	memcpy(after_colon, colon + 1, black_len - prefixLen - 1);

	sprintf(markup, "%s<span foreground=\"%s\">:</span>%s", before_colon, colon_colour, after_colon);
	pango_layout_set_markup(layout, markup, -1);
	pango_layout_get_pixel_size(layout, &b_pix_width, &b_pix_height);

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

	cairo_restore(buff_crt);

	// debug clock text bounds
//	pango_layout_set_text(layout, white, -1);
//	pango_layout_get_pixel_size(layout, &pix_width, &pix_height);
//	double rx = .5 * (wi / 2.0f - pix_width);
//	double ry = .5 * (hi - pix_height);
//	cairo_set_source_rgb(buff_crt, 1, 1, 1);
//	cairo_rectangle(buff_crt, rx, ry, pix_width, pix_height);
//	cairo_set_line_width(buff_crt, 1);
//	cairo_stroke(buff_crt);

	// paint separator on boundary to avoid aliasing
	cairo_set_source_rgb(buff_crt, (w_bg_r + b_bg_r) / 2.0, (w_bg_g + b_bg_g) / 2.0, (w_bg_b + b_bg_b) / 2.0);

	cairo_move_to (buff_crt, halfWidth, 0);
	cairo_line_to(buff_crt, halfWidth, hi);
	cairo_set_line_width(buff_crt, 1.0f);
	cairo_stroke(buff_crt);

	// Apply cache surface to crt
	cairo_set_source_surface(crt, buffer_surf, 0.0f, 0.0f);
	cairo_paint(crt);

	// Cleanup
	g_object_unref(layout);
	cairo_destroy(buff_crt);
	cairo_surface_destroy(buffer_surf);
	pthread_mutex_unlock(&mutex_drawing);
}

static void clock_face_class_init(ClockFaceClass *class) {
	GtkWidgetClass *widget_class;
	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->draw = draw_clock_face;
}

static void clock_face_init(ClockFace *clock_face) {}

void clock_face_set_clock(ClockFace *clock_face, chess_clock *clock) {
	clock_face->clock = clock;
	if (clock) {
		clock->parent = GTK_WIDGET(clock_face);
	}
}

GtkWidget *clock_face_new(void) {
	return g_object_new(TYPE_CLOCK_FACE, NULL);
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
	pthread_mutex_lock(&mutex_drawing);
	double half_w = gtk_widget_get_allocated_width(clock) / 2.0;
	int x_offset = (int) (black ? floor(half_w) : 0);
	int redraw_w = (int) ceil(half_w);
	gtk_widget_queue_draw_area(clock, x_offset, 0, redraw_w, gtk_widget_get_allocated_height(clock));
	pthread_mutex_unlock(&mutex_drawing);
}