#ifndef UCIADAPTER_H_
#define UCIADAPTER_H_

int spawn_uci_engine(void);
void write_to_uci(char *message);
void user_move_to_uci(char *move);
void start_new_uci_game(int time);

#endif /* UCIADAPTER_H_ */
