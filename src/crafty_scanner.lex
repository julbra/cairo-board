%{
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "src/crafty_scanner.h"
#include "src/cairo-board.h"


%}


%%

^"move ".* {
	//debug("Matched move from Crafty\n");
	return CRAFTY_MOVED;
}

^"Hint: ".* {
/*
Hint: Nxh7
*/
	//debug("Matched Hint from Crafty\n");
	return CRAFTY_HINT;
}

^[ \t]*[0-9]+.* {
/*
        10    -83     363 569078  3. ... Nc6 4. Nf3 Bc5 5. Bd3 O-O 6. O-O d6 7. Nc3 Be6 8. Kh1
*/
	return CRAFTY_PONDERED;
}

[\n]+ {
	return LINE_FEED;
}

. {
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

