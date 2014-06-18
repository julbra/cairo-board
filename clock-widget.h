#ifndef __CLOCK_FACE_H__
#define __CLOCK_FACE_H__

#include <gtk/gtk.h>
#include "clocks.h"

G_BEGIN_DECLS

#define TYPE_CLOCK_FACE		 (clock_face_get_type ())
#define CLOCK_FACE(obj)		 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOCK_FACE, ClockFace))
#define CLOCK_FACE_CLASS(obj)	 (G_TYPE_CHECK_CLASS_CAST ((obj), CLOCK_FACE,  ClockFaceClass))
#define IS_CLOCK_FACE(obj)	 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOCK_FACE))
#define IS_CLOCK_FACE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_CLOCK_FACE))
#define CLOCK_FACE_GET_CLASS	 (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOCK_FACE, ClockFaceClass))


typedef struct _ClockFace	ClockFace;
typedef struct _ClockFaceClass	ClockFaceClass;

struct _ClockFace {
	GtkDrawingArea parent;
	/* the chess_clock displayed by this widget */
	chess_clock *clock; 
};

struct _ClockFaceClass {
	GtkDrawingAreaClass parent_class;
};

G_END_DECLS

GtkWidget *clock_face_new (void);
void clock_face_set_clock (ClockFace *clock_face, chess_clock *clock);
gboolean periodic_refresh (gpointer data);
void refresh_both_clocks (GtkWidget *clock);
void refresh_one_clock(GtkWidget *clock, int black);
GType clock_face_get_type (void);


#endif

