%{
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "src/san_scanner.h"
#include "src/cairo-board.h"

%}

delim		[ \t]
whitesp		{delim}+
column		[a-h]
row			[1-8]
piecechar	[RBNQKP]


%%

{piecechar}?{column}{row}[xX:-]?{column}{row}(=?\(?{piecechar}\)?)? {
	/* Short form move with column AND row disambiguator e.g. Nb1d7 */
	//debug("Short form with column AND row disambiguator %s\n", yytext);
	int skip1 = 0;
	int skip2 = 0;

	/* get the piece type */
	type = char_to_type(main_game->whose_turn, yytext[0]);
	if (type == -1) {
		type = (main_game->whose_turn? B_PAWN: W_PAWN);
		skip1--;
	}

	/* remove the [xX:-] */
	if ((yytext[2+skip1] == 'x') || (yytext[2+skip1] == 'X')) {
		skip2++;
	}

	/* get promo char */
	if ((yytext[4+skip1+skip2] == '=')) {
		main_game->promo_type = char_to_type(main_game->whose_turn, yytext[5+skip1+skip2]);
		debug("Promo to %c\n", yytext[5+skip1+skip2]);
	}

	currentMoveString[0] = yytext[1+skip1];
	currentMoveString[1] = yytext[2+skip1];
	currentMoveString[2] = yytext[3+skip1+skip2];
	currentMoveString[3] = yytext[4+skip1+skip2];
	currentMoveString[4] = '\0';
    
	return 1;
}

{piecechar}?{column}[xX:-]?{column}{row}(=?\(?{piecechar}\)?)? {
	/* Short form move with column disambiguator e.g. Nbd7 or Nbxf6 cxb5 */
	//debug("Short form with column disambiguator %s\n", yytext);
	int skip1 = 0;
	int skip2 = 0;

	/* get the piece type */
	type = char_to_type(main_game->whose_turn, yytext[0]);
	if (type == -1) {
		type = (main_game->whose_turn? B_PAWN: W_PAWN);
		skip1--;
	}

	/* remove the [xX:-] */
	if ((yytext[2+skip1] == 'x') || (yytext[2+skip1] == 'X')) {
		skip2++;
	}

	/* get promo char */
	if ((yytext[4+skip1+skip2] == '=')) {
		main_game->promo_type = char_to_type(main_game->whose_turn, yytext[5+skip1+skip2]);
		debug("Promo to %c\n", yytext[5+skip1+skip2]);
	}

	currentMoveString[0] = yytext[1+skip1];
	currentMoveString[1] = '1'-1;
	currentMoveString[2] = yytext[2+skip1+skip2];
	currentMoveString[3] = yytext[3+skip1+skip2];
	currentMoveString[4] = '\0';
    
	return 1;
}

{piecechar}?{row}[xX:-]?{column}{row}(=?\(?{piecechar}\)?)? {
	/* Short form move with row disambiguator e.g. N1d3 */
	//debug("Short form with row disambiguator %s\n", yytext);
	int skip1 = 0;
	int skip2 = 0;

	/* get the piece type */
	type = char_to_type(main_game->whose_turn, yytext[0]);
	if (type == -1) {
		type = (main_game->whose_turn? B_PAWN: W_PAWN);
		skip1--;
	}

	/* remove the [xX:-] */
	if ((yytext[2+skip1] == 'x') || (yytext[2+skip1] == 'X')) {
		skip2++;
	}

	/* get promo char */
	if ((yytext[4+skip1+skip2] == '=')) {
		main_game->promo_type = char_to_type(main_game->whose_turn, yytext[5+skip1+skip2]);
		debug("Promo to %c\n", yytext[5+skip1+skip2]);
	}

	currentMoveString[0] = 'a'-1;
	currentMoveString[1] = yytext[1+skip1];
	currentMoveString[2] = yytext[2+skip1+skip2];
	currentMoveString[3] = yytext[3+skip1+skip2];
	currentMoveString[4] = '\0';
    
	return 1;
}

{piecechar}?[xX:-]?{column}{row}(=?\(?{piecechar}\)?)? {
	/* Short form move e.g. Nc3 or Pe4 or Nxf6 */
	//debug("Short form %s\n", yytext);
	int skip = 0;

	/* get the piece type */
	type = char_to_type(main_game->whose_turn, yytext[0]);
	if (type == -1) {
		type = (main_game->whose_turn? B_PAWN: W_PAWN);
		skip--;
	}

	/* remove the [xX:-] */
	if ((yytext[1+skip] == 'x') || (yytext[1+skip] == 'X')) {
		skip++;
	}

	/* get promo char */
	if ((yytext[3+skip] == '=')) {
		main_game->promo_type = char_to_type(main_game->whose_turn, yytext[4+skip]);
		debug("Promo to %c\n", yytext[4+skip]);
	}

	currentMoveString[0] = yytext[1+skip];
	currentMoveString[1] = yytext[2+skip];
	currentMoveString[2] = '\0';
    
	return 1;
}

\[White[ \t\n]*\"[^"]*\"\] {
	debug("Found White Name Tag: %s\n", yytext);
	char *begin = strchr(yytext, '"')+1;
	char *end = strrchr(yytext, '"');
	size_t length = (end-begin)/sizeof(char);
	strncpy(main_game->white_name, begin, length);
	main_game->white_name[length] = '\0';
	// skip tags for now
	return 2;
}

\[Black[ \t\n]*\"[^"]*\"\] {
	debug("Found Black Name Tag: %s\n", yytext);
	char *begin = strchr(yytext, '"')+1;
	char *end = strrchr(yytext, '"');
	size_t length = (end-begin)/sizeof(char);
	strncpy(main_game->black_name, begin, length);
	main_game->black_name[length] = '\0';
	// skip tags for now
	return 2;
}

\[WhiteElo[ \t\n]*\"[^"]*\"\] {
	debug("Found White Elo Tag: %s\n", yytext);
	char *begin = strchr(yytext, '"')+1;
	char *end = strrchr(yytext, '"');
	size_t length = (end-begin)/sizeof(char);
	if (length > 0) {
		strncpy(main_game->white_rating, begin, length);
	}
	else {
		memset(main_game->white_rating, 0, 32);
	}
	// skip tags for now
	return 2;
}

\[BlackElo[ \t\n]*\"[^"]*\"\] {
	debug("Found Black Elo Tag: %s\n", yytext);
	char *begin = strchr(yytext, '"')+1;
	char *end = strrchr(yytext, '"');
	size_t length = (end-begin)/sizeof(char);
	if (length > 0) {
		strncpy(main_game->black_rating, begin, length);
	}
	else {
		memset(main_game->black_rating, 0, 32);
	}
	// skip tags for now
	return 2;
}


\[[A-Za-z0-9][A-Za-z0-9_+#=-]*[ \t\n]*\"[^"]*\"\] {
	debug("Found Tag: %s\n", yytext);
	// skip tags for now
	return 2;
}

00|0-0|oo|OO|o-o|O-O {
	// king-side castle
	if (!main_game->whose_turn) { // white
		type = W_KING;
		currentMoveString[0] = 'g';
		currentMoveString[1] = '1';
		currentMoveString[2] = '\0';
	}
	else {
		type = B_KING;
		currentMoveString[0] = 'g';
		currentMoveString[1] = '8';
		currentMoveString[2] = '\0';
	}
	return 1;
}

000|0-0-0|ooo|OOO|o-o-o|O-O-O   {
	// queen-side castle
	if (!main_game->whose_turn) { // white
		type = W_KING;
		currentMoveString[0] = 'c';
		currentMoveString[1] = '1';
		currentMoveString[2] = '\0';
	}
	else {
		type = B_KING; // black
		currentMoveString[0] = 'c';
		currentMoveString[1] = '8';
		currentMoveString[2] = '\0';
	}
	return 1;
}

[0-2/]+-[0-2/]+ {
	debug("Found end token: %s\n", yytext);
	return MATCHED_END_TOKEN;
}

[*]{whitesp}*\n {
	debug("Found game unfinished token: %s\n", yytext);
	return MATCHED_END_TOKEN;
}

[0-9]+\. {
	//printf("Move %s\n", yytext);
}

[\n]+ {
	//printf("Skipped linefeed\n");
}

. {
	//printf("Skipped %s\n", yytext);
        /* Skip everything else */
}

<<EOF>> {
	debug("Reached EOF\n");
	return SAN_EOF_TYPE;
}

%%

int yywrap() {
	return 1;
}


