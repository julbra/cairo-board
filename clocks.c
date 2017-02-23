#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>

#include "clocks.h"
#include "clock-widget.h"

#define CLOCK_INTERVAL 100000 // 100ms

extern int debug_flag;
#ifndef debug
#ifdef colour_console
void debug(format, ...) { if (debug_flag) fprintf(stdout, "%s:\033[31m%d\033[0m " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define debug(format, ...) if (debug_flag) fprintf(stdout, "%s:%d " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

void send_to_ics(char*);

static struct timeval zero_tv = {0, -250000}; // allow 250ms margin

void *black_clock_runner_function(void *_clock) {
	chess_clock *clock = (chess_clock*)_clock;
	struct timeval now, diff;

	for(;;) {
		sem_wait( &clock->sem_black );
		sem_post( &clock->sem_black );

		/* get the current time */
		gettimeofday(&now, NULL);

		/* check the clock has not just resumed from pause */
		if (clock->last_modified_time[1].tv_sec) {
			/* measure exactly how long elapsed since last_update */
			timersub( &now, &clock->last_modified_time[1], &diff);

			/* thread safe update of the remaining time */
			{
				pthread_mutex_lock( &clock->update_mutex );
				timersub( &clock->remaining_time[1], &diff, &clock->remaining_time[1]);
				pthread_mutex_unlock( &clock->update_mutex );
			}
			
		}

		gdk_threads_enter();
		if (clock->parent != NULL) {
			refresh_one_clock(GTK_WIDGET(clock->parent), 1);
		}
		gdk_threads_leave();

		/* update last modified timestamp */
		clock->last_modified_time[1] = now;

		/* sleep approx 100ms */
		usleep(CLOCK_INTERVAL);
	}
}


void *white_clock_runner_function(void *_clock) {
	chess_clock *clock = (chess_clock*)_clock;
	struct timeval now, diff;

	for(;;) {
		sem_wait( &(clock->sem_white) );
		sem_post( &clock->sem_white );

		/* get the current time */
		gettimeofday(&now, NULL);

		/* check the clock has not just resumed from pause */
		if (clock->last_modified_time[0].tv_sec) {
			/* measure exactly how long elapsed since last_update */
			timersub( &now, &clock->last_modified_time[0], &diff);

			/* thread safe update of the remaining time */
			{
				pthread_mutex_lock( &clock->update_mutex );
				timersub( &clock->remaining_time[0], &diff, &clock->remaining_time[0]);
				pthread_mutex_unlock( &clock->update_mutex );
			}

		}

		if (clock->parent != NULL) {
			gdk_threads_enter();
			refresh_one_clock(GTK_WIDGET(clock->parent), 0);
			gdk_threads_leave();
		}

		/* update last modified timestamp */
		clock->last_modified_time[0] = now;

		/* sleep approx 100ms */
		usleep(CLOCK_INTERVAL);
	}
}

chess_clock *clock_new(int initial_time_s, int incerement_s, int relation) {

	chess_clock *clock = malloc(sizeof(chess_clock));
	if (!clock) {
		perror("Malloc failed");
		return NULL;
	}
	clock->relation = relation;

	// Set initial values into the clock
	clock->remaining_time[0].tv_sec = initial_time_s;
	clock->remaining_time[0].tv_usec = 0;

	clock->remaining_time[1].tv_sec = initial_time_s;
	clock->remaining_time[1].tv_usec = 0;

	/* setting this field to 0 is
	 * our convention to say that the clock was stopped */
	clock->last_modified_time[0].tv_sec = 0;
	clock->last_modified_time[1].tv_sec = 0;

	sem_init( &clock->sem_white, 0, 0); // locked state
	sem_init( &clock->sem_black, 0, 0); // locked state
	pthread_mutex_init(&clock->update_mutex, NULL);// = PTHREAD_MUTEX_INITIALIZER;

	// Spawn both white and black runner threads
	// (both in blocked state)
	pthread_create( &clock->white_clock_runner, NULL, white_clock_runner_function, clock);
	pthread_create( &clock->black_clock_runner, NULL, black_clock_runner_function, clock);
	return clock;
}

void clock_freeze(chess_clock *clock) {
	if (is_active(clock, 0)) {
		stop_one_clock(clock, 0, true);
	}
	if (is_active(clock, 1)) {
		stop_one_clock(clock, 1, true);
	}
}

void clock_reset(chess_clock *clock, int initial_time, int increment, int relation, bool should_lock) {
	clock_freeze(clock);
	clock->initial_time = initial_time;
	clock->increment = increment;
	clock->relation = relation;
	update_clocks(clock, clock->initial_time, clock->initial_time, should_lock);
}

void clock_destroy(chess_clock *clock) {
	pthread_cancel( clock->white_clock_runner );
	pthread_cancel( clock->black_clock_runner );
	pthread_join( clock->white_clock_runner, NULL);
	pthread_join( clock->black_clock_runner, NULL);

	sem_post( &clock->sem_white );
	sem_destroy( &clock->sem_white );
	sem_post( &clock->sem_black );
	sem_destroy( &clock->sem_black );
	pthread_mutex_unlock( &clock->update_mutex );
	pthread_mutex_destroy( &clock->update_mutex );

	free(clock);
}

/* this is thread safe */
void update_clocks(chess_clock *clock, int white_s, int black_s, bool shouldLock) {
	pthread_mutex_lock( &clock->update_mutex );
	clock->remaining_time[0].tv_sec = white_s;
	clock->remaining_time[0].tv_usec = 0;
	clock->remaining_time[1].tv_sec = black_s;
	clock->remaining_time[1].tv_usec = 0;
	pthread_mutex_unlock( &clock->update_mutex );
	if (shouldLock) {
		gdk_threads_enter();
	}
	refresh_both_clocks(GTK_WIDGET(clock->parent));
	if (shouldLock) {
		gdk_threads_leave();
	}
}

/* locks the associated runner function and reset the mtime */
void stop_one_clock(chess_clock *clock, int color, bool shouldLock) {

	sem_wait(color ? &clock->sem_black : &clock->sem_white);

	pthread_mutex_lock(&clock->update_mutex);
	clock->last_modified_time[color].tv_sec = 0;
	pthread_mutex_unlock(&clock->update_mutex);

	if (shouldLock) {
		gdk_threads_enter();
	}
	refresh_one_clock(GTK_WIDGET(clock->parent), color);
	if (shouldLock) {
		gdk_threads_leave();
	}
}

/* unlocks the associated runner funtion */
void start_one_clock(chess_clock *clock, int color) {
	sem_post(color ? &clock->sem_black : &clock->sem_white);
}

int is_active(chess_clock *clock, int color) {
	int ret;
	sem_getvalue(color ? &clock->sem_black : &clock->sem_white, &ret);
	return ret;
}

void swap_clocks(chess_clock *clock, bool shouldLock) {
	int w, b;
	w = is_active(clock, 0);
	b = is_active(clock, 1);
	if ( w && b) {
		fprintf(stderr, "ERROR: swap_clocks - both white and black semaphores are unlocked!\n");
		return;
	}
	if ( !w && !b) {
		fprintf(stderr, "ERROR: swap_clocks - both white and black semaphores are locked!\n");
		return;
	}
	stop_one_clock(clock,  b ? 1 : 0, shouldLock);
	start_one_clock(clock, b ? 0 : 1);
}

void start_one_stop_other_clock(chess_clock *clock, int color_to_start, bool shouldLock) {
	debug("start_one_stop_other_clock\n");
	int to_start = color_to_start ? 1 : 0;
	int to_stop  = color_to_start ? 0 : 1;
	if (!is_active(clock, to_start)) {
		start_one_clock(clock, to_start);
	}
	if (is_active(clock, to_stop)) {
		stop_one_clock(clock, to_stop, shouldLock);
	}
}

long get_remaining_time(chess_clock *clock, int color) {
	long millis = tv_to_ms(&clock->remaining_time[color]);
	if (millis < 0) {
		millis = 0;
	}
	return millis;
}

void print_clock(chess_clock *clock) {
	char black_time[32], white_time[32];
	ms_to_string(tv_to_ms(&clock->remaining_time[0]), white_time);
	ms_to_string(tv_to_ms(&clock->remaining_time[1]), black_time);
	fprintf(stdout, "\rWhite time: [%s] - Black time: [%s]\n", white_time, black_time);
}

long tv_to_ms(struct timeval *tv) {
	long res = 0;
	res += 1000 * (tv->tv_sec);
	res += (tv->tv_usec) / 1000;
	return res;
}

void ms_to_string(long ms, char human[]) {

	if (ms > (long) 24 * 60 * 60 * 1000) { // > 1 day
		fprintf(stderr, "ERROR: %ld milliseconds passed to %s exceeded allowed range of 1day", ms, __FUNCTION__);
		return;
	}

	// prepare for use with strcat
	memset(human, 0, strlen(human) + 1);

	if (ms < 0) {
		strcat(human, "-");
		ms = -ms;
	}
	if (ms >= 60*60*1000) { // > 1 hour
		char hours[4];
		sprintf(hours, "%ldh", (ms/3600000)%24);
		strcat(human, hours);
	}
	if (ms >= 60*1000) { // > 1 min
		char minutes[4];
		sprintf(minutes, "%ldm", (ms/60000)%60);
		strcat(human, minutes);
	}
	if (ms >= 1000) { // > 1 sec
		char seconds[6];
		sprintf(seconds, "%ld.%lds", (ms/1000)%60, (ms/100)%10);
		strcat(human, seconds);
	}
	else {
		char milli_seconds[6];
		sprintf(milli_seconds, "%ldms", ms);
		strcat(human, milli_seconds);
	}
	return;
}

void clock_to_string(chess_clock *clock, int color, char clock_string[], char ghost_string[]) {

	long ms = get_remaining_time(clock, color);

	if (ms > (long) 24 * 60 * 60 * 1000) { // > 1 day
		fprintf(stderr, "ERROR: %ld milliseconds passed to %s exceeded allowed range of 1day", ms, __FUNCTION__);
		return;
	}

	// prepare for use with strcat
	memset(clock_string, 0, strlen(clock_string) + 1);
	memset(ghost_string, 0, strlen(ghost_string) + 1);
//	strcat(clock_string, color ? "Black: " : "White: ");
//	strcat(ghost_string, "88888: ");

	if (ms < 0) {
		strcat(clock_string, "-");
		strcat(ghost_string, "-");
		ms = -ms;
	}

	if (ms >= 60 * 60 * 1000) { // > 1 hour
		char hours[4];
		sprintf(hours, "%ld:", (ms / 3600000) % 24);
		strcat(clock_string, hours);
		size_t prev_len = strlen(ghost_string);
		size_t hours_len = strlen(hours);
		memset(ghost_string + prev_len, '8', hours_len - 1);
		ghost_string[prev_len + hours_len - 1] = ':';
	}
	if (ms >= 10 * 1000) { // > 10 seconds
		char minutes[4];
		sprintf(minutes, "%02ld:", (ms / 60000) % 60);
		strcat(clock_string, minutes);
		strcat(ghost_string, "88:");
		char seconds[3];
		sprintf(seconds, "%02ld", (ms / 1000) % 60);
		strcat(clock_string, seconds);
		strcat(ghost_string, "88");
	} else {
		char seconds[9];
		sprintf(seconds, "00:%02ld.%ld", (ms / 1000) % 60, (ms / 100) % 10);
		strcat(clock_string, seconds);
		strcat(ghost_string, "88:88.8");
	}
	return;
}

int is_clock_expired(chess_clock *clock, int color) {
	return timercmp( &clock->remaining_time[color], &zero_tv, < );
}

int am_low_on_time(chess_clock *clock) {
	if (!clock->relation) return 0;
	long my_time =       get_remaining_time(clock, (clock->relation > 0 ? 0 : 1));
	long opponent_time = get_remaining_time(clock, (clock->relation > 0 ? 1 : 0));
	if (opponent_time > 0) {
		return 1000 * my_time / opponent_time < 667; // my time is less than 2/3 of my opponent's
	}
	return 0;
}

int clock_main(void) {

	chess_clock *myclock;
	myclock = clock_new(1*10, 0, 0);
	start_one_clock(myclock, 0);

	int i;
	for(i=0; i<40; i++) {
		print_clock(myclock);
		usleep(100000);
	}

	swap_clocks(myclock, true);
	for(i=0; i<40; i++) {
		print_clock(myclock);
		usleep(100000);
	}

	swap_clocks(myclock, true);
	for(i=0; i<40; i++) {
		print_clock(myclock);
		usleep(100000);
	}

	swap_clocks(myclock, true);
	for(i=0; i<40; i++) {
		print_clock(myclock);
		usleep(100000);
	}
	swap_clocks(myclock, true);
	for(i=0; i<40; i++) {
		print_clock(myclock);
		usleep(100000);
	}
	swap_clocks(myclock, true);
	for(i=0; i<40; i++) {
		print_clock(myclock);
		usleep(100000);
	}
	clock_destroy(myclock);

	return 0;

}

