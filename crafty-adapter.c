#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "crafty-adapter.h"
#include "crafty_scanner.h"
#include "cairo-board.h"

#define BSIZE 512


int crafty_in;
int crafty_out;
int crafty_err;

int crafty_scanner__scan_bytes(const char*, int length);

void write_to_crafty(char *message) {
	if (write(crafty_in, message, strlen(message)) == -1) {
		perror(NULL);
	}
}

pthread_t crafty_read_thread;

void *parse_crafty_function( void *ptr );

int spawn_crafty(void) {
	GPid child_pid;

	int i;

	// FIXME: fix that shite!
	gchar **argv = calloc(10, sizeof(char*));
	for (i=0; i<10; i++) {
		argv[i] = calloc(128, sizeof(char));
	}
	argv[0] = "/usr/games/bin/crafty";
	argv[1] = NULL;

	const char* home_dir = g_get_home_dir();
    char *crafty_directory = calloc(strlen(home_dir) + 9, sizeof(char));
	strcpy(crafty_directory, home_dir);
	strcat(crafty_directory, "/.crafty");

	g_spawn_async_with_pipes(crafty_directory, argv, NULL, 0, NULL, NULL, &child_pid,
			&crafty_in, &crafty_out, &crafty_err, NULL);

	free(crafty_directory);

	pthread_create( &crafty_read_thread, NULL, parse_crafty_function, (void*)(&i));
	write_to_crafty("xboard\n");
	write_to_crafty("post\n");


//	for (i=0; i<10; i++) {
//			free(argv[i]);
//	}
//	free(argv);

	return 0;
}

void parse_crafty_buffer(void) {

	char raw_buff[512];

	memset(raw_buff, 0, 512);
	int nread = read(crafty_out, &raw_buff, 512);
	if (nread < 1) {
		fprintf(stderr, "ERROR: failed to read data from Crafty pipe\n");
	}
	crafty_scanner__scan_bytes(raw_buff, nread);
	int i = 0;
	while (i > -1) {
		i = crafty_scanner_lex();
		switch(i) {
		case CRAFTY_MOVED: {
			set_last_move(crafty_scanner_text+5);
			g_signal_emit_by_name(board, "got-crafty-move");
//			char *command = calloc(256, sizeof(char));
//			snprintf(command, 256, "%s\n", crafty_scanner_text+5);
//			send_to_ics(command);
//			printf("Crafty moves: %s", command);
//			free(command);
			break;
		}
		case CRAFTY_PONDERED: {
			char *buf = crafty_scanner_text;
			while (*buf == ' ') buf++;
			printf("Crafty thinks: %s\n", buf);
			break;
		}
		case CRAFTY_HINT: {
			printf("Crafty hints: %s\n", crafty_scanner_text+6);
			break;
		}
		case LINE_FEED:
		case EMPTY_LINE:
		default:
			break;
		}
	}
}

void *parse_crafty_function(void *ptr) {

	while(is_running_flag()) {
		parse_crafty_buffer();
	}

	fprintf(stdout, "[parse Crafty thread] - Closing Crafty parser\n");
	return 0;
}
