#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <string.h>

#include "clock-widget.h"

static const char *FONT_FACE = "Sans Bold";
extern GtkWidget *board;
static pthread_mutex_t mutex_drawing = PTHREAD_MUTEX_INITIALIZER;

G_DEFINE_TYPE (ClockFace, clock_face, GTK_TYPE_DRAWING_AREA) ;

//static void clock_face_size_request (GtkWidget *clock_face, GtkRequisition *requisition) {
//	printf("%s\n", __func__);
//	int min_height = 50;
//	requisition->height = min_height;
//}
//
//
//static void clock_face_size_allocate (GtkWidget *widget, GtkAllocation *allocation) {
//
//	ClockFace *clock_face;
//
//	printf("%s\n", __func__);
//
//	widget->allocation = *allocation;
//	int bw = board->allocation.width;
//	if (widget->allocation.width > bw) widget->allocation.width = bw;
//
//	//printf("this alloc %d - board alloc %d\n", allocation->width, bh);
//
//	//int font_size = bh/20;
//	//widget->allocation.height = font_size;
//	//if (clock_face->allocation.height < font_size + 5) {
//	//	clock_face->allocation.height = font_size + 5;
//	//}
//	if (GTK_WIDGET_REALIZED (widget)) {
//		clock_face = CLOCK_FACE (widget);
//		gdk_window_move_resize (widget->window, allocation->x, allocation->y, bw, allocation->height);
//		//dial->radius = MAX(allocation->width,allocation->height) * 0.45;
//		//dial->pointer_width = dial->radius / 5;
//	}
//}


/*static gboolean clock_face_configure(GtkWidget *clock_face, GdkEventConfigure *event) {
	printf("%s\n", __func__);

	//int width = event->width;
	//printf("Configure passed to clock-widget: new width = %d\n", width);

	return FALSE;
}
*/

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
	char black[32];

	clock_to_string(cf->clock, 0, white);
	clock_to_string(cf->clock, 1, black);

	PangoFontDescription *desc;
	PangoLayout *layout;

	layout = pango_cairo_create_layout (buff_crt);
	pango_layout_set_text(layout, white, -1);

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
	hi_font_size = hi/1.58;
	wi_font_size = wi/17.45;
	font_size = (hi_font_size < wi_font_size)? hi_font_size : wi_font_size;

	sprintf(font_str, "%s %.1f", FONT_FACE, font_size);
	desc = pango_font_description_from_string (font_str);
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);

	int pix_width, pix_height;
	pango_layout_get_pixel_size (layout, &pix_width, &pix_height);

	/* Check that pixel width is no larger than a half-clock width
	 * and that pixel height is no larger than a clock height */
	while ( (pix_height > hi || pix_width > wi) && font_size > 0) {
		font_size -= .1;
		sprintf(font_str, "%s %.2f", FONT_FACE, font_size);
		desc = pango_font_description_from_string (font_str);
		pango_layout_set_font_description (layout, desc);
		pango_font_description_free (desc);
		pango_layout_get_pixel_size (layout, &pix_width, &pix_height);
	}

	int wa = is_active(cf->clock, 0);
	int ba = is_active(cf->clock, 1);
	int my_color = -1;
	int warn_me = 0;
	static int warn_toggle = 1;

	if (cf->clock->relation > 0) {
		my_color = 0;
	}
	else if (cf->clock->relation < 0) {
		my_color = 1;
	}

	if (my_color > -1) {
		if (my_color ? ba : wa) {
			warn_me = am_low_on_time(cf->clock);
			warn_toggle++; warn_toggle %= 4;
		}
	}

	cairo_save(buff_crt);

	// paint white's clock background
	if (wa) {
		if (warn_me && warn_toggle < 2) {
			cairo_set_source_rgb(buff_crt, .86, 0, 0); // bright red
		}
		else {
			cairo_set_source_rgb(buff_crt, 0, .4, .7);
		}
	}
	else {
		cairo_set_source_rgb(buff_crt, 1, 1, 1);
	}
	cairo_rectangle(buff_crt, 0, 0, ((double)wi/2.0f), hi);
	cairo_fill(buff_crt);

	// paint white's clock text
	if (wa) {
		cairo_set_source_rgb(buff_crt, 1, 1, 1);
	}
	else {
		cairo_set_source_rgb(buff_crt, 0, .4, .7);
	}

	double tx = .5*(wi/2.0f - pix_width);
	double ty = .5*(hi - pix_height);

	cairo_translate (buff_crt, tx, ty);
	pango_cairo_show_layout (buff_crt, layout);


	cairo_restore(buff_crt);

	// paint black's clock background
	cairo_translate (buff_crt, ((double)wi/2.0f), 0);

	if (ba) {
		if (warn_me && warn_toggle < 2) {
			cairo_set_source_rgb(buff_crt, .86, 0, 0); // bright red
		}
		else {
			cairo_set_source_rgb(buff_crt, 0, .4, .7);
		}
	}
	else {
		cairo_set_source_rgb(buff_crt, 1, 1, 1);
	}
	cairo_rectangle(buff_crt, 0, 0, ((double)wi/2.0f), hi);
	cairo_fill(buff_crt);

	// paint black's clock text
	if (ba) {
		cairo_set_source_rgb(buff_crt, 1, 1, 1);
	}
	else {
		cairo_set_source_rgb(buff_crt, 0, .4, .7);
	}

	pango_layout_set_text(layout, black, -1);
	pango_layout_get_pixel_size (layout, &pix_width, &pix_height);
	tx = .5*(wi/2.0f - pix_width);
	cairo_translate (buff_crt, tx, ty);
	pango_cairo_show_layout (buff_crt, layout);

	g_object_unref (layout);

	cairo_destroy(buff_crt);

	// Apply cache surface to crt
	cairo_set_source_surface (crt, buffer_surf, 0.0f, 0.0f);
	cairo_paint(crt);

	cairo_surface_destroy(buffer_surf);

	pthread_mutex_unlock( &mutex_drawing );
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
	int half_w = clock->allocation.width/2;
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

