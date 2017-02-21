%{
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "uci_scanner.h"
#include "cairo-board.h"


%}


%%

^"uciok" {
    debug("UCI: All options sent, engine ready in UCI mode\n");
    return UCI_OK;
}

^"readyok" {
    debug("UCI: Engine ready\n");
    return UCI_READY;
}

^"id name ".* {
    debug("UCI: Engine ID name from UCI engine %s\n", yytext);
    return UCI_ID_NAME;
}

^"id author ".* {
    debug("UCI: Engine ID author from UCI engine %s\n", yytext);
    return UCI_ID_AUTHOR;
}

^"option name ".*" type ".* {
    //debug("UCI: Engine supports: %s\n", yytext);
    return UCI_OPTION;
}

^"info depth ".* {
	debug("UCI: info depth: %s\n", yytext);
	return UCI_INFO_DEPTH;
}

^"bestmove ".*" ponder ".* {
    debug("UCI: best move and ponder: %s\n", yytext);
    return UCI_BEST_MOVE_WITH_PONDER;
}

^"bestmove ".* {
    debug("UCI: best move: %s\n", yytext);
    return UCI_BEST_MOVE;
}

[\n]+ {
	return LINE_FEED;
}

^.* {
    debug("Not yet matched from UCI engine: %s\n", yytext);
	return UNMATCHED;
}

<<EOF>> {
	//debug("Reached EOF\n");
	return EOF_TYPE;
}

%%

int yywrap() {
	return 1;
}

