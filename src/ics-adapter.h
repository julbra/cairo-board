#ifndef CAIRO_BOARD_ICS_ADAPTER_H
#define CAIRO_BOARD_ICS_ADAPTER_H

#include <stdbool.h>

bool invalid_password;
int ics_fd;

int init_ics(void);
void cleanup_ics(void);
bool check_board12_game_consistency(void);

#endif //CAIRO_BOARD_ICS_ADAPTER_H
