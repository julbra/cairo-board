/*
 * channels.h
 *
 *  Created on: 10 Dec 2009
 *      Author: hts
 */

#ifndef CHANNELS_H_
#define CHANNELS_H_

extern GHashTable *channel_map;
extern GHashTable *reverse_channel_map;
extern GSList *my_channels;

extern GtkWidget *channels_notebook;
extern GtkTextTagTable *tags_table;
extern GtkTextTag *blue_fg_tag;

extern const char *channel_descriptions[];

void insert_text_channel_view(int channel_num, char *username, char *message, gboolean should_lock_threads);
void free_all_channels(void);
void sort_my_channels(void);
int count_my_channels(void);
void show_my_channels(void);
void show_one_channel(int show_this);
gboolean is_in_my_channels(int num);
void parse_my_channels_line(char *message);
void handle_channel_removed(int removed_channel);
void handle_channel_added(int added_channel);

#endif /* CHANNELS_H_ */
