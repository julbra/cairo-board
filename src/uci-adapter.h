#ifndef UCIADAPTER_H_
#define UCIADAPTER_H_

typedef enum {
	ENGINE_ANALYSIS,
	ENGINE_WHITE,
	ENGINE_BLACK
} UCI_MODE;

void cleanup_uci(void);
int spawn_uci_engine(bool brainfish);
void write_to_uci(char *message);
void user_move_to_uci(char *move, bool analyse);
void start_new_uci_game(unsigned int time, UCI_MODE mode);
void start_uci_analysis(void);

#endif /* UCIADAPTER_H_ */
