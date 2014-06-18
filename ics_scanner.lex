%{
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "ics_scanner.h"
#include "cairo-board.h"


%}

delim		[ \t]
whitesp		{delim}+
row			[1-8]
username	[A-Za-z]+
userattributes ("("[A-Z*][A-Z]?")")*
will_echo	"\377\373\001"
wont_echo	"\377\374\001"

%%

^"login: " {
	return LOGIN_PROMPT;
}

^"Press return to enter the server as ".* {
	return CONFIRM_GUEST_LOGIN_PROMPT;
}

^"password: " {
	return PASSWORD_PROMPT;
}

"Starting FICS session as "[a-zA-Z]+ {
	yytext += 25;
	//debug("matched login '%s'\n", yytext);
	return GOT_LOGIN;
}

^"fics% "+ {
	/* ignore FICS prompt */
	//debug("matched fics prompt '%s'\n", yytext);
	return FICS_PROMPT;
}

^<12>[ ].*[\n]* {
	return BOARD_12;
}

^"Creating: "[A-Za-z]+" ("[ ]*[0-9+-]+") "[A-Za-z]+" ("[ ]*[0-9+-]+") ".* {
/* catch first 'creating' message to get ratings
 * Message is of the form :
 * "Creating: julbra (1911) lesio ( 866) rated blitz 3 0"
 * NB: names on FICS can only consist of lower and upper case letters */
	debug("caught Create message %s\n", ics_scanner_text);
	return CREATE_MESSAGE;
}

^"Challenge: "[A-Za-z]+" ("[ ]*[0-9+-]+") ""[black] "?"[white] "?[A-Za-z]+" ("[ ]*[0-9+-]+") ".* {
/*
Challenge: GuestMZPD (----) GuestKYYK (----) unrated standard 15 2.
Challenge: GuestJRBS (----) [black] GuestSBNL (----) unrated blitz 5 0.
*/
	return CHALLENGE;
}

^"{Game "[0-9]+" ("[A-Za-z]+" vs. "[A-Za-z]+") "[^}]+"} "[-/0-2*]+ {
/*
{Game 221 (julbra vs. Przemekchess) Przemekchess checkmated} 1-0
*/
	return GAME_END;
}

^"{Game "[0-9]+" ("[A-Za-z]+" vs. "[A-Za-z]+") Creating "[^}]+"}" {
/*
{Game 221 (julbra vs. Przemekchess) Creating rated blitz match.}
*/
	return GAME_START;
}

^"{Game "[0-9]+" ("[A-Za-z]+" vs. "[A-Za-z]+") Continuing "[^}]+"}" {
/*
{Game 19 (jennyh vs. julbra) Continuing rated blitz match.}
*/
	return GAME_RESUME;
}

^"Movelist for game "[0-9]+: {
	return MOVE_LIST_START;
}

^"----  ----------------   ----------------" {
	return MOVE_LIST_UNDERLINE;
}

^[ \t]*[0-9]+"."[ \t]*[-0Ooa-hx1-8RBNQKP+#=]+[ \t]*[()0-9:]+[ \t]*$ {
/*
  1.  Nf3     (0:00)
*/  
	debug("Matched white ply from Movelist\n");
	return MOVE_LIST_WHITE_PLY;
}

^[ \t]*[0-9]+"."[ \t]*[-0Ooa-hx1-8RBNQKP+#=]+[ \t]*[()0-9:]+[ \t]*[-0Ooa-hx1-8RBNQKP+#=]+[ \t]*[()0-9:]+[ \t]*$ {
/*
  4.  exd5    (0:00)     exd5    (0:05)  
*/  
	debug("Matched complete move from Movelist\n");
	return MOVE_LIST_FULL_MOVE;
}

^"      {Still in progress} *" {
	return MOVE_LIST_END;
}

^"You are now observing game "[0-9]+"." {
	return OBSERVE_START;
}

^"Game "[0-9]+": "[A-Za-z]+" ("[ ]*[0-9+-]+") "[A-Za-z]+" ("[ ]*[0-9+-]+") ".* {
/*
Game 127: ImAGoon (1910) FDog (2025) rated standard 15 0
*/
	debug("matched OBSERVE_HEADER '%s'\n", yytext);
	return OBSERVE_HEADER;
}

^"**** Invalid password! ****" {
	return INVALID_PASSWORD;
}


^{username}{userattributes}"("[0-9]+"): ".* {
/*
Frubes(50): good day, how do you do
*/
	return CHANNEL_CHAT;
}

^{username}{userattributes}" tells you: ".* {
/*
jennyh tells you: good day, how do you do
*/
	return PRIVATE_TELL;
}

^"<pf> "[0-9]+" w="{username}" t="[a-z]+" p=".* {
/*
<pf> 38 w=jennyh t=draw p=#
<pf> 31 w=jennyh t=match p=jennyh (----) [black] julbra (----) unrated standard 30 12
*/
	debug ("Matched OFFER FROM\n");
	return OFFER_FROM;
}

^"<pr> "[0-9]+ {
/*
<pr> 31
*/
	debug ("Matched OFFER_REMOVED\n");
	return OFFER_REMOVED;
}

^"<pt> "[0-9]+ {
/*
TODO
*/
	debug ("Matched OFFER_TO\n");
	return OFFER_TO;
}

"-- channel list: "[0-9]+" channels --" {
/*
-- channel list: 29 channels --
*/
	debug ("Matched MY_CHANNELS_HEADER '%s'\n", yytext);
	return MY_CHANNELS_HEADER;
}

^([0-9]+{whitesp}?)+$ {
/*
1    3    8    49   53   69
*/
	debug ("Matched MY_CHANNELS_LINE '%s'\n", yytext);
	return MY_CHANNELS_LINE;
}

^"["[0-9]+"] removed from your channel list." {
/*
[238] removed from your channel list.
*/
	return CHANNEL_REMOVED;
}

^"["[0-9]+"] added to your channel list." {
/*
[28] added to your channel list.
*/
	return CHANNEL_ADDED;
}

{will_echo} {
	return WILL_ECHO;
}

{wont_echo} {
	return WONT_ECHO;
}

^"\\   ".* {
	fprintf(stderr, "Matched Line continued symbol!!! Is iv_nowrap ivariable set and locked?\n");
	return LINE_CONTINUED;
}   

[\n][\n]+ {
	return EMPTY_LINES;
}

[\n]+ {
	return UNMATCHED;
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

