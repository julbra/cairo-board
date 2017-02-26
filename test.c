#include <stdlib.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <string.h>

#include "drawing-backend.h"
#include "cairo-board.h"
#include "chess-backend.h"
#include "channels.h"


gboolean test_animate_random_step(gpointer data);

void test_random_animation(void) {
	struct timeval tv; // C requires "struct timval" instead of just "timeval"
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	g_timeout_add(1000/240, test_animate_random_step, &main_game->white_set[QUEEN]);
	g_timeout_add(1000/240, test_animate_random_step, &main_game->black_set[QUEEN]);
}

gboolean test_random_flip(gpointer trash) {
	usleep(rand()%(10000));
	if (rand()%2) {
		g_signal_emit_by_name(board, "flip_board");
	}
//	auto_move(&black_set[PAWN5], 4, 4, 0, AUTO_SOURCE);
	int i;
		char w_name[28];
		char b_name[28];
		memset(w_name, 0, 28);
		memset(b_name, 0, 28);

		for (i=0; i<10+(rand()%18); i++) {
			if (i!= 0 && !(rand()%6)) {
				w_name[i] = ' ';
			}
			else {
				w_name[i] = 'a'+(rand()%26);
			}
			if (i!= 0 && !(rand()%6)) {
				b_name[i] = ' ';
			}
			else {
				b_name[i] = 'a'+(rand()%26);
			}
		}
	start_game(w_name, b_name, 60, 0, (rand() % 2) ? 1 : -1, false);
//	auto_move(&white_set[PAWN5], 4, 3, 0, MANUAL_SOURCE);
//	set_last_move("e4");
//	g_signal_emit_by_name(board, "got_move");

	return TRUE;
}

void test_crazy_flip(void) {
	struct timeval tv; // C requires "struct timval" instead of just "timeval"
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	g_timeout_add(1000/2, test_random_flip, NULL);
}

static gboolean random_channel_insert(gpointer trash) {
	char *random_string = malloc(256*sizeof(char));
	snprintf(random_string, 256, "%2d: Test Message String \"\"needs to ::be:: \"long\" e:@#]['*&^$£!':' and all", rand()%100);
	insert_text_channel_view(rand()%5+10, "Bozo", random_string, true);
	free(random_string);
	return TRUE;
}

void test_random_channel_insert(void) {
	g_timeout_add(1000/3, random_channel_insert, NULL);
}

extern GtkWidget *moves_list_title_label;
extern GtkWidget *main_window;

static gboolean random_title(gpointer trash) {
	gdk_threads_enter();
	char header[256];
	memset(header, 0, 256);
	int i;
	char w_name[28];
	char b_name[28];
	memset(w_name, 0, 28);
	memset(b_name, 0, 28);

	for (i=0; i<10+(rand()%18); i++) {
		if (i!= 0 && !(rand()%6)) {
			w_name[i] = ' ';
		}
		else {
			w_name[i] = 'a'+(rand()%26);
		}
		if (i!= 0 && !(rand()%6)) {
			b_name[i] = ' ';
		}
		else {
			b_name[i] = 'a'+(rand()%26);
		}
	}
	sprintf(header, "%s\nvs\n%s", w_name, b_name);
	gtk_label_set_text(GTK_LABEL(moves_list_title_label), header);

	char buff[256];
			char title[256];
			sprintf(buff, "%s vs %s", w_name, b_name);
			sprintf(title, "cairo-board - %s", buff);
			gtk_window_set_title(GTK_WINDOW(main_window), title);

	gdk_threads_leave();

	return TRUE;
}

static gboolean observe_cairo_guest_one(gpointer trash);

static gboolean unobserve_cairo_guest_one(gpointer trash) {
	send_to_ics("unobserve cairoguestone\n");
	g_timeout_add(2500, observe_cairo_guest_one, NULL);
	return FALSE;
}

static gboolean observe_cairo_guest_one(gpointer trash) {
	send_to_ics("observe cairoguestone\n");
	g_timeout_add(2000, unobserve_cairo_guest_one, NULL);
	return FALSE;
}


void test_random_title(void) {
	g_timeout_add(1000, random_title, NULL);
}

void test_observe(void) {
	g_timeout_add(15000, observe_cairo_guest_one, NULL);
}
