Cairo-Board
===========

Cairo-Board is a chess interface for Linux, written in C with GTK+ 3 and
Cairo. It can be used to play on an Internet Chess Server (FICS), to analyse
a position with a UCI engine, or to replay games stored in a PGN file.

Original author: Julien Bramary.
License: GNU General Public License, version 3 (see LICENSE.md).


Features
--------

  * Resizable chessboard drawn with Cairo, with SVG pieces and animated
    moves.
  * Pieces can be moved by drag and drop or by clicking the source square
    and then the destination square.
  * Full legality checking: castling, en passant, promotion (with a menu to
    choose the promotion piece), check, checkmate, stalemate, fifty-move
    rule and threefold repetition.
  * Highlighting of the selected piece, of its legal moves and of the last
    move; warning when a king is in check.
  * Moves list in standard algebraic notation, optionally with Unicode
    figurines, and buttons to show the first, previous, next and last move.
  * Opening recognition: the ECO code and the name of the opening are
    displayed, using the file full_eco.idx.
  * Chess clocks with a seven-segment display.
  * FICS compatible ICS client: login dialog (login and password can be
    remembered, automatic login), playing and observing games, pre-moves,
    challenges (accept/decline dialog), channel and private chats shown in
    tabs.
  * UCI engine support: the "Computer Analysis" panel shows the score, the
    depth, the speed (kN/s), the engine name and the best line.
  * Automatic replay of the games of a PGN file.
  * Board colours can be set on the command line.

Mouse:

  left button      move a piece
  right button     flip the board
  middle button    reset the board and the game


Building
--------

Requirements: CMake (3.6 or newer), a C compiler, Flex, and the development
files of GTK+ 3, librsvg 2, FreeType, Fontconfig and GLib (gthread).

    mkdir build
    cd build
    cmake ..
    make

The executable is named cairo_board and is written in the root directory of
the project (not in the build directory).


Running
-------

Cairo-Board looks for its data files in the current directory:

    cairo-board.css          style sheet of the interface
    full_eco.idx             openings index
    DSEG7Classic-Bold.ttf    font of the clocks
    themes/commons/          pieces (SVG)

The program must therefore be started from the directory that contains
these files:

    cd /path/to/cairo-board
    ./cairo_board [OPTIONS]

A UCI engine is always started at launch. If the engine cannot be started,
the program aborts; use --engine if Stockfish is not installed as
/usr/bin/stockfish.

Without --ics and without --load, the program starts in analysis mode: the
moves played on the board are sent to the engine, which analyses the
current position.


Command-line options
--------------------

Options may be written with one or two dashes (-ics or --ics) and may be
abbreviated as long as the abbreviation is unambiguous. The value of an
option may be given as --option=VALUE or as --option VALUE. There is no
--help option; unknown options are reported and ignored.

General:

  --debug, --d
        Print debug information on the standard output.

  --fig
        Use Unicode chess figurines instead of piece letters in the moves
        list. This requires a UTF-8 locale (see LANG, LC_CTYPE, LC_ALL);
        if a figurine cannot be printed, the option is disabled.

  --engine=ENGINE_PATH
        Path of the UCI engine to start.
        Default: /usr/bin/stockfish

Internet Chess Server:

  --ics
        ICS mode: connect to an Internet Chess Server and show the login
        dialog.

  --icshost=HOST
        Host name of the server. Only used with --ics.
        Default: freechess.org

  --icsport=PORT
        Port of the server. Only used with --ics.
        Default: 5000

  --guest
        Automatically confirm the prompt displayed by the server when
        logging in as a guest.

PGN files:

  --load=FILE
        Load a PGN file and replay automatically all the games it
        contains, one after the other. Sample files are provided in the
        pgn directory.

  --delay=MILLISECONDS
        Delay between two moves, and between two games, when replaying a
        PGN file.
        Default: 1000

  --gamenum=NUMBER
        Number of the game to load in the PGN file. This option is
        accepted but currently has no effect.

Board colours:

  --lr=VALUE, --lg=VALUE, --lb=VALUE
        Red, green and blue components (0 to 255) of the light squares.
        Default: 239, 239, 239

  --dr=VALUE, --dg=VALUE, --db=VALUE
        Red, green and blue components (0 to 255) of the dark squares.
        Default: 109, 129, 179

  The highlight colours are computed from these values.

Test options:

  --first, --login1=HANDLE, --login2=HANDLE
        Options reserved for tests. They are accepted but currently have
        no effect.

Examples:

    ./cairo_board
    ./cairo_board --engine=/usr/games/stockfish
    ./cairo_board --ics
    ./cairo_board --ics --icshost=freechess.org --icsport=5000 --fig
    ./cairo_board --load=pgn/Kasparov.pgn --delay=500
    ./cairo_board --lr=240 --lg=217 --lb=181 --dr=181 --dg=136 --db=99


Configuration file
------------------

The configuration file is

    $XDG_CONFIG_HOME/cairo-board/cairo-board.conf

that is, by default,

    ~/.config/cairo-board/cairo-board.conf

The directory and the file are created at the first launch, readable and
writable by their owner only. The file is read at startup and written
after each successful login to the chess server. It only stores the
settings of the login dialog; the command-line options are not saved in it.

It is a key file (INI format) made of groups and "key=value" lines:

    [global]
    author = Julien Bramary

    [Login]
    save_login=true
    login=myhandle
    password=...
    auto_login=false

Group [global]:

  author
        Written when the file is created. Not used by the program.

Group [Login]:

  save_login    (boolean, default: false)
        State of the "Remember me" check box. If true, the login and the
        password are saved and filled in the login dialog.

  login         (string)
        Handle used to log in to the chess server.

  password      (string)
        Password of the account. It is only obfuscated, NOT encrypted:
        anyone who can read the file can recover the password.

  auto_login    (boolean, default: false)
        State of the "Automatically login" check box. If true, the login
        is attempted at startup without waiting for the user. It requires
        save_login to be true.

Because the password is stored in an obfuscated form, it should not be
edited by hand: use the login dialog to change it. To forget the saved
login, uncheck "Remember me" at the next login, or delete the [Login]
group, or delete the file.
