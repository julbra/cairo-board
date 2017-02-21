#ifndef __CAIRO_CHESS_CLOCKS_H__
#define __CAIRO_CHESS_CLOCKS_H__

#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct _chess_clock {
	int initial_time; // in seconds
	int increment; // in seconds
	struct timeval remaining_time[2]; // timeval values [0] is white_time, [1] is black_time
	struct timeval last_modified_time[2]; // timeval values [0] is white_time, [1] is black_time
	pthread_t white_clock_runner;
	pthread_t black_clock_runner;
	sem_t sem_white;
	sem_t sem_black;
	pthread_mutex_t update_mutex;

	/* my relation to this clock: 
	 *  0 -> observe
	 *  1 -> play white
	 * -1 -> play black */
	int relation;

	/* a reference to the widget displaying this clock if any */
	GtkWidget *parent;

} chess_clock;

/* Prototypes */
chess_clock *clock_new(int initial_time_s, int incerement_s, int relation);
void clock_destroy(chess_clock *);
void clock_reset(chess_clock *clock, int initial_time, int increment, int relation);
void clock_freeze(chess_clock *clock);
void *black_clock_runner_function(void *);
void *white_clock_runner_function(void *);
void start_one_clock(chess_clock *, int);
void stop_one_clock(chess_clock *, int, bool);
void swap_clocks(chess_clock *clock, bool);
void start_one_stop_other_clock(chess_clock *clock, int color_to_start, bool);
int is_active(chess_clock *, int);
int is_clock_expired(chess_clock *clock, int color);
void print_clock(chess_clock *);
long tv_to_ms(struct timeval *);
void ms_to_string(long, char[]);
void clock_to_string(chess_clock *, int, char[]);
void update_clocks(chess_clock *, int, int, bool);
long get_remaining_time(chess_clock *, int);
int am_low_on_time(chess_clock *clock);
void set_parent_widget(GtkWidget *parent);

#endif

