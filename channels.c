#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "cairo-board.h"

#define CHANNEL_BUFFER_MAX_LINES 256

const char *channel_descriptions[] = {
"Admins  [restricted to admins]", /*0*/
"Server Help and Assistance",
"General discussions about FICS",
"FICS programmers",
"Guests",
"Service Representatives [restricted]",
"Help with Interface and timeseal questions",
"OnlineTours", /*7*/
"","","","","","","","","","","","",
"Forming Team games", /*20*/
"Playing team games",
"Playing team games",
"Forming Simuls", /*23*/
"","","","","","",
"Books and Knowledge", /*30*/
"Computer Games",
"Movies",
"Quacking  &  Other Duck Topics",
"Sports",
"Music",
"Mathematics  &  Physics",
"Philosophy",
"Literature  &  Poetry",
"Politics", /*39*/
"","","","","","","","",
"Mamer managers [restricted to mamer managers]", /*48*/
"Mamer tournament channel",
"The Chat channel",
"The Youth channel",
"The Old Timers channel",
"The Guest Chat channel",
"",
"The Chess channel",
"Beginner Chess",
"Discussions on coaching and teaching chess",
"Chess Books",
"",
"Chess Openings/Theory", /*60*/
"Chess Endgames",
"Blindfold Chess channel",
"Chess Advisors [restricted]",
"Computer Chess",
"Special Events channel",
"Examine channel (people willing to analyze games with you)",
"Lecture channel (for special lecture programs)",
"Ex-Yugoslav",
"Latin",
"Finnish",
"Scandinavian (Danish, Norwegian, Swedish)",
"German",
"Spanish",
"Italian",
"Russian",
"Dutch",
"French",
"Greek",
"Icelandic",
"Chinese",
"Turkish",
"Portuguese",
"General computer discussions",
"Macintosh/Apple",
"Unix/Linux",
"DOS/Windows 3.1/95/NT",
"VMS",
"Programming discussions", /*88*/
"",
"The STC BUNCH (players who like Slow Time Controls: 30- to 120-minute games)	[see \"help stc\"]",
"Suicide Chess channel",
"Wild Chess channel",
"Bughouse Chess channel",
"Gambit channel (players who like to play gambit openings)",
"Scholastic Chess channel",
"College Chess channel",
"Crazyhouse Chess channel",
"Losers Chess channel",
"Atomic Chess channel",
"Trivia"
};

GHashTable *channel_map;
GHashTable *reverse_channel_map;
GSList *my_channels = NULL;

GtkWidget *channels_notebook;
GtkWidget *right_split_pane;
GtkTextTagTable *tags_table;
GtkTextTag *blue_fg_tag;

typedef struct {
	int num;
	GtkWidget *top_vbox;
	GtkWidget *text_view;
	GtkWidget *scrolled_window;
	GtkTextBuffer *text_view_buffer;
	GtkWidget *text_entry;
	GtkWidget *scroll_lock;
} channel;

void free_channel_function(gpointer key, gpointer value, gpointer data);
void leave_channel_function(gpointer key, gpointer value, gpointer data);
void show_channel_function(gpointer key, gpointer value);
void radio_callback(GtkWidget *item, gpointer value);
void join_channel_function(gpointer key, gpointer value);

channel *get_channel(int channel_number);
channel *get_active_channel(void);


GtkWidget *create_append_image_menu_item(GtkWidget *menu, const gchar *stock_id, const gchar *str) {
	GtkWidget* item = gtk_image_menu_item_new_with_label(str);
	GtkWidget *icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	return item;
}

gint chan_compare_func(gconstpointer a, gconstpointer b) {
	return ((channel*)a)->num - ((channel*)b)->num;
}

static GtkWidget *switch_to_channel_menu(channel *chan) {
	GSList *group = NULL;
	GtkWidget *switch_to_channel_menu = gtk_menu_new();
	int active = get_active_channel()->num;
	GSList *chans = my_channels;
	while(chans) {
		int next_chan = GPOINTER_TO_INT(chans->data);
		gboolean activated = (g_hash_table_lookup(channel_map, &next_chan) != NULL);
		char *str = calloc(256, sizeof(char));
		if (next_chan < 101 && strcmp("", channel_descriptions[next_chan])) {
			sprintf(str, "%d: %s", next_chan, channel_descriptions[next_chan]);
		}
		else {
			sprintf(str, "Channel %d", next_chan);
		}

		GtkWidget* item = gtk_radio_menu_item_new_with_label(group, str);
		if (activated) {
			char *markup;
			markup = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>", str);
			GtkWidget *child = gtk_bin_get_child(GTK_BIN(item));
			gtk_label_set_markup(GTK_LABEL (child), markup);
			g_free(markup);
		}

		free(str);

		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
		gtk_menu_shell_append(GTK_MENU_SHELL(switch_to_channel_menu), item);

		if (next_chan == active) {
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(item), TRUE);
		}

		g_signal_connect(GTK_OBJECT (item), "toggled", G_CALLBACK (radio_callback), GINT_TO_POINTER(next_chan));

		chans = g_slist_next(chans);
	}
	return switch_to_channel_menu;
}

static void popup_tab_menu(channel *channel, guint32 time) {
	GtkWidget *channel_tab_popup = gtk_menu_new();

	GtkWidget *join_item = create_append_image_menu_item(channel_tab_popup, GTK_STOCK_ADD, "Join new channel");
	g_signal_connect (GTK_OBJECT (join_item), "activate", G_CALLBACK (join_channel_function), NULL);

	gtk_menu_shell_append(GTK_MENU_SHELL(channel_tab_popup), gtk_separator_menu_item_new());

	GtkWidget *switch_item = create_append_image_menu_item(channel_tab_popup, GTK_STOCK_JUMP_TO, "Switch/Activate channel...");
	GtkWidget *switch_sub = switch_to_channel_menu(channel);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(switch_item), switch_sub);

	gtk_menu_shell_append(GTK_MENU_SHELL(channel_tab_popup), gtk_separator_menu_item_new());

	char item_str[32];
	snprintf(item_str, 32, "Close channel %d", channel->num);
	GtkWidget *close_item = create_append_image_menu_item(channel_tab_popup, GTK_STOCK_CLOSE, item_str);
	g_signal_connect (GTK_OBJECT (close_item), "activate", G_CALLBACK (free_channel_function), channel);

	snprintf(item_str, 32, "Leave channel %d", channel->num);
	GtkWidget *leave_item = create_append_image_menu_item(channel_tab_popup, GTK_STOCK_QUIT, item_str);
	g_signal_connect (GTK_OBJECT (leave_item), "activate", G_CALLBACK (leave_channel_function), channel);

	gtk_widget_show_all(channel_tab_popup);
	gtk_menu_popup(GTK_MENU(channel_tab_popup), NULL, NULL, NULL, NULL, 3, time);
}

static gboolean on_button_press(channel *chan, GdkEventButton *pButton, GtkWidget *pWidget) {

	if (pButton->type == GDK_BUTTON_PRESS) {

		if (pButton->button == 3) {
			debug("Right click on tab channel %d\n", chan->num);
			popup_tab_menu(chan, pButton->time);
		}
	}
	return FALSE;
}

GtkWidget *create_channel_view(void) {
	/* moves list text view widget */
	GtkWidget *channel_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(channel_view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(channel_view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(channel_view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_justification(GTK_TEXT_VIEW(channel_view), GTK_JUSTIFY_LEFT);

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(channel_view), 10);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(channel_view), 10);

	return channel_view;
}

/* This created a scrolled window to wrap the passed widget */
GtkWidget *create_add_scrolled_window(GtkWidget *widget) {
	GtkWidget *scrolled_view = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_view), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(scrolled_view), widget);
	return scrolled_view;
}


gboolean channel_entry_callback(GtkWidget *entry, channel *channel) {
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
	int length = strlen(text);
	if (length > 0) {
		char *command = calloc(length+11, sizeof(char));
		sprintf(command, "tell %d %s\n", channel->num, text);
		send_to_ics(command);
		free(command);
		gtk_entry_set_text(GTK_ENTRY(entry), "");
	}
	return FALSE;
}

gboolean set_lock_scrolling_tooltip(GtkWidget *lock_button) {
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lock_button))) {
		gtk_widget_set_tooltip_text(lock_button, "Unlock scrolling");
	}
	else {
		gtk_widget_set_tooltip_text(lock_button, "Lock scrolling");
	}
	return FALSE;
}

gint get_channel_index(channel *channel) {
	return gtk_notebook_page_num(GTK_NOTEBOOK(channels_notebook), channel->top_vbox);
}

channel *get_active_channel(void) {
	int active_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(channels_notebook));
	GtkWidget *active_vbox = gtk_notebook_get_nth_page(GTK_NOTEBOOK(channels_notebook), active_page);
	channel *channel = g_hash_table_lookup(reverse_channel_map, active_vbox);
	return channel;
}

/* removes the channel from the tabbed pane and frees up associated resources */
void free_channel(channel *channel) {
	gtk_notebook_remove_page(GTK_NOTEBOOK(channels_notebook), get_channel_index(channel));
	g_hash_table_remove(channel_map, &(channel->num));
	g_hash_table_remove(reverse_channel_map, channel->top_vbox);
	free(channel);
	if (!gtk_notebook_get_n_pages(GTK_NOTEBOOK(channels_notebook))) {
		gtk_widget_hide(channels_notebook);
	}
}

void free_channel_function(gpointer key, gpointer value, gpointer data) {
	free_channel(value);
}

void join_channel_function(gpointer key, gpointer value) {
	popup_join_channel_dialog(FALSE);
}

void leave_channel_function(gpointer key, gpointer value, gpointer data) {
	channel *chan = (channel*)value;
	char *command = calloc(128, sizeof(char));
	snprintf(command, 128, "-chan %d\n", chan->num);
	send_to_ics(command);
	free(command);
}

gint sort_ints(gconstpointer a, gconstpointer b) {
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

void sort_my_channels(void) {
	my_channels = g_slist_sort(my_channels, sort_ints);
}

int count_my_channels(void) {
	int count_them = 0;
	GSList *chans = my_channels;
	while(chans) {
		chans = g_slist_next(chans);
		count_them++;
	}
	return count_them;
}

gboolean is_in_my_channels(int num) {
	GSList *chans = my_channels;
	while(chans) {
		int next_chan = GPOINTER_TO_INT(chans->data);
		if (next_chan == num) {
			return TRUE;
		}
		chans = g_slist_next(chans);
	}
	return FALSE;
}

/*
1    3    8    49   53   69
*/
void parse_my_channels_line(char *message) {
	char *next;
	int chan_num;
	while (true) {
		chan_num = strtol(message, &next, 10);
		if (next == message) {
			break;
		}
		message = next;
		my_channels = g_slist_prepend(my_channels, GINT_TO_POINTER(chan_num));
	}
}

void handle_channel_removed(int removed_channel) {

	GSList *chans = my_channels;
	while(chans) {
		int next_chan = GPOINTER_TO_INT(chans->data);
		if (next_chan == removed_channel) {
			debug("Channel %d removed\n", removed_channel);
			my_channels = g_slist_delete_link(my_channels, chans);
			break;
		}
		chans = g_slist_next(chans);
	}
	channel *to_remove = g_hash_table_lookup(channel_map, &removed_channel);
	if (to_remove) {
		free_channel(to_remove);
	}
}

void handle_channel_added(int added_channel) {
	my_channels = g_slist_insert_sorted(my_channels, GINT_TO_POINTER(added_channel), sort_ints);
	gdk_threads_enter();
	show_channel_function(NULL, GINT_TO_POINTER(added_channel));
	gdk_threads_leave();
}

void show_one_channel(int show_this) {
	if (is_in_my_channels(show_this)) {
		gdk_threads_enter();
		show_channel_function(NULL, GINT_TO_POINTER(show_this));
		gdk_threads_leave();
	}
}

void show_my_channels(void) {
	GSList *chans = my_channels;
	gdk_threads_enter();
	while(chans) {
		int next_chan = GPOINTER_TO_INT(chans->data);
		show_channel_function(NULL, GINT_TO_POINTER(next_chan));
		chans = g_slist_next(chans);
	}
	gdk_threads_leave();
}

void show_channel_function(gpointer key, gpointer value) {
	int channel_num = GPOINTER_TO_INT(value);
	channel *channel = get_channel(channel_num);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(channels_notebook), get_channel_index(channel));
}

void radio_callback(GtkWidget *item, gpointer value) {
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
		int channel_num = GPOINTER_TO_INT(value);
		channel *channel = get_channel(channel_num);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(channels_notebook), get_channel_index(channel));
	}
}

void free_all_channels(void) {
	g_hash_table_foreach(channel_map, free_channel_function, NULL);
}

/* allocates a new channel and create associated widgets
 * call free_channel() to free up the allocated resources */
channel* create_channel(int channel_num) {

	if (!gtk_widget_get_visible(channels_notebook)) {
		gtk_widget_show(channels_notebook);
	}

	channel *new_channel = malloc(sizeof(channel));
	new_channel->num = channel_num;
	/* create channel window */
	new_channel->text_view = create_channel_view();
	new_channel->scrolled_window = create_add_scrolled_window(new_channel->text_view);
	new_channel->text_view_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW (new_channel->text_view));

	/* Label for text entry */
	char label_str[16];
	snprintf(label_str, 16, "tell %d:", channel_num);
	GtkWidget *entry_label = gtk_label_new(label_str);

	/* Text entry */
	new_channel->text_entry =  gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(new_channel->text_entry), 400);
	g_signal_connect(GTK_OBJECT(new_channel->text_entry), "activate", G_CALLBACK(channel_entry_callback), new_channel);

	/* Scroll lock button */
	new_channel->scroll_lock = gtk_toggle_button_new();
	GtkWidget *button_image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_MENU);
	gtk_button_set_image(GTK_BUTTON(new_channel->scroll_lock), button_image);
	gtk_button_set_relief(GTK_BUTTON(new_channel->scroll_lock), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click(GTK_BUTTON(new_channel->scroll_lock), FALSE);
	gtk_widget_set_tooltip_text(new_channel->scroll_lock, "Lock scrolling");
	g_signal_connect(new_channel->scroll_lock, "clicked", G_CALLBACK(set_lock_scrolling_tooltip), NULL);

	/* Text entry hbox */
	GtkWidget *entry_hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_set_spacing(GTK_BOX(entry_hbox), 5);
	gtk_box_pack_start(GTK_BOX(entry_hbox), entry_label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(entry_hbox), new_channel->text_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(entry_hbox), new_channel->scroll_lock, FALSE, FALSE, 0);

	/* Tab vbox */
	new_channel->top_vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(new_channel->top_vbox), new_channel->scrolled_window, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(new_channel->top_vbox), entry_hbox, FALSE, FALSE, 0);

	/* right gravity */
	GtkTextIter start_bozo, end_iter;
	gtk_text_buffer_get_bounds(new_channel->text_view_buffer, &start_bozo, &end_iter);

	/* create the end mark */
	gtk_text_buffer_create_mark(new_channel->text_view_buffer, "end_bookmark", &end_iter, 0);

	/* Link tag table */
	gtk_text_buffer_create_tag(new_channel->text_view_buffer, "blue_fg", "foreground", "blue", NULL);

	char lab_text[16];
	snprintf(lab_text, 16, "Ch[%d]", channel_num);
	GtkWidget *tab_label_box = gtk_hbox_new(FALSE, 10);
	GtkWidget *tab_label = gtk_label_new(lab_text);
	GtkWidget *tab_close_button = gtk_button_new();
	GtkWidget *tab_close_button_icon = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_button_set_relief(GTK_BUTTON(tab_close_button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click(GTK_BUTTON(tab_close_button), FALSE);
	gtk_button_set_image(GTK_BUTTON(tab_close_button), tab_close_button_icon);

	gtk_widget_set_name( tab_close_button, "tab-close-button" );
	g_signal_connect (G_OBJECT (tab_close_button), "clicked", G_CALLBACK(free_channel_function), new_channel);

	GtkWidget *tab_event_box = gtk_event_box_new();

	/* make event box background transparent */
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(tab_event_box), FALSE);

	gtk_box_set_spacing(GTK_BOX(tab_label_box), 5);
	gtk_box_pack_start(GTK_BOX(tab_label_box), tab_label, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(tab_label_box), tab_close_button, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(tab_event_box), tab_label_box);

	g_signal_connect_swapped(G_OBJECT (tab_event_box), "button-press-event", G_CALLBACK (on_button_press), new_channel);

	gtk_widget_show_all(tab_event_box);

	gtk_widget_show_all(new_channel->top_vbox);


	char *extended_description = calloc(256, sizeof(char));;
	if (channel_num < 101 && strcmp("", channel_descriptions[channel_num])) {
		sprintf(extended_description, "%d: %s", channel_num, channel_descriptions[channel_num]);
	}
	else {
		sprintf(extended_description, "Channel %d", channel_num);
	}
	GtkWidget *tab_menu_label = gtk_label_new(extended_description);
	gtk_widget_set_tooltip_text(tab_event_box, extended_description);
	free(extended_description);

	/* Find the insertion point comparing channel_numbers */
	gint insertion_index = G_MAXINT;
	GList *chan_list = g_hash_table_get_values(channel_map);
	GList *next = chan_list;
	while(next) {
		channel *next_chan = ((channel*)next->data);
		if (channel_num < next_chan->num) {
			gint ind = get_channel_index(next_chan);
			if (insertion_index > ind ) {
				insertion_index = ind;
			}
		}
		next = g_list_next(next);
	}
	g_list_free(chan_list);

	gint index = gtk_notebook_insert_page_menu(GTK_NOTEBOOK(channels_notebook), new_channel->top_vbox, tab_event_box, tab_menu_label, insertion_index);
	if (index < 0) {
		fprintf(stderr, "Failed to insert channel tab into notebook!\n");
	}
	debug("chan_num %d - real index %d\n", channel_num, index);
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(channels_notebook), new_channel->top_vbox, FALSE);

	g_hash_table_insert(channel_map, &(new_channel->num), new_channel);
	g_hash_table_insert(reverse_channel_map, new_channel->top_vbox, new_channel);

	return new_channel;
}

/* returns a reference to the channel
 * it will create a new one if it doesn't exist*/
channel *get_channel(int channel_number) {
	channel *channel = g_hash_table_lookup(channel_map, &channel_number);
	if (!channel) {
		debug("Creating channel %d\n", channel_number);
		channel = create_channel(channel_number);
	}
	return channel;
}

/* Append message at the end of the sample channel buffer
 * NB: message must be NULL terminated*/
void insert_text_channel_view(int channel_num, char *username, char *message, gboolean should_lock_threads) {

	char *final_username;
	char *final_message;

	if (should_lock_threads) {
		gdk_threads_enter();
	}

	channel *channel = get_channel(channel_num);

	if (!GTK_IS_TEXT_VIEW(channel->text_view)) {
		// Killed? tough!
		return;
	}

	/* append the text at the end of the buffer */
	GtkTextMark *end_mark = gtk_text_buffer_get_mark(channel->text_view_buffer, "end_bookmark");
	if (!end_mark) {
		fprintf(stderr, "Failed to get the mark at the end of the channel buffer!\n");
		return;
	}

	final_username = calloc(strlen(username)+2, sizeof(char));
	final_message = calloc(strlen(message)+3, sizeof(char));
	sprintf(final_username, "%s:", username);
	sprintf(final_message, " %s\n", message);

	GtkTextIter mark_it;
	gtk_text_buffer_get_iter_at_mark(channel->text_view_buffer, &mark_it, end_mark);
	gtk_text_buffer_insert_with_tags_by_name(channel->text_view_buffer, &mark_it, final_username,-1, "blue_fg",NULL);
	gtk_text_buffer_get_iter_at_mark(channel->text_view_buffer, &mark_it, end_mark);
	gtk_text_buffer_insert(channel->text_view_buffer, &mark_it, final_message, -1);

	/* Count lines to check we're not full */
	int linecount = gtk_text_buffer_get_line_count(channel->text_view_buffer);
	if (linecount > CHANNEL_BUFFER_MAX_LINES) {
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds(channel->text_view_buffer, &start, &end);
		gtk_text_buffer_get_iter_at_line(channel->text_view_buffer, &end, linecount - CHANNEL_BUFFER_MAX_LINES -1);
		gtk_text_buffer_delete(channel->text_view_buffer, &start, &end);
	}

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(channel->scroll_lock))) {
		/* autoscroll to the end by making our end mark visible */
		gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(channel->text_view), end_mark, 0, 0, 0, 0);
	}

	/* Make the tab active */
	//gtk_notebook_set_current_page(GTK_NOTEBOOK(channels_notebook), channel->index);

	if (should_lock_threads) {
		gdk_threads_leave();
	}

	free(final_username);
	free(final_message);
}


