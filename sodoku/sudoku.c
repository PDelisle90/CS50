// Implements the game of Sudoku

#include "sudoku.h"
#include <ctype.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


// macro for processing control characters
#define CTRL(x) ((x) & ~0140)

// size of each int (in bytes) in *.bin files
#define INTSIZE 4


// wrapper for our game's globals
struct
{
    // the current level
    char *level;

    // the game's board
    int board[9][9];

    // Initial board
    int initialBoard[9][9];

    // the board's number
    int number;

    // the board's top-left coordinates
    int top, left;

    // the cursor's current location between (0,0) and (8,8)
    int y, x;
} g;


// prototypes
void draw_grid(void);
void draw_borders(void);
void draw_logo(void);
void draw_numbers(void);
void hide_banner(void);
bool load_board(void);
void handle_signal(int signum);
void log_move(int ch);
void redraw_all(void);
bool restart_game(void);
void show_banner(char *b);
void show_cursor(void);
void shutdown(void);
bool startup(void);
bool checkColumns(int);
bool checkRows(int);
bool checkSquares(int);
bool wonGame(void);
void warning(void);

/*
 * Main driver for the game.
 */

int
main(int argc, char *argv[])
{
    // ensure that number of arguments is as expected
    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: sudoku n00b|l33t [#]\n");
        return 1;
    }

    // ensure that level is valid
    if (strcmp(argv[1], "debug") == 0)
        g.level = "debug";
    else if (strcmp(argv[1], "n00b") == 0)
        g.level = "n00b";
    else if (strcmp(argv[1], "l33t") == 0)
        g.level = "l33t";
    else
    {
        fprintf(stderr, "Usage: sudoku n00b|l33t [#]\n");
        return 2;
    }

    // n00b and l33t levels have 1024 boards; debug level has 9
    int max = (strcmp(g.level, "debug") == 0) ? 9 : 1024;

    // ensure that #, if provided, is in [1, max]
    if (argc == 3)
    {
        // ensure n is integral
        char c;
        if (sscanf(argv[2], " %d %c", &g.number, &c) != 1)
        {
            fprintf(stderr, "Usage: sudoku n00b|l33t [#]\n");
            return 3;
        }

        // ensure n is in [1, max]
        if (g.number < 1 || g.number > max)
        {
            fprintf(stderr, "That board # does not exist!\n");
            return 4;
        }

        // seed PRNG with # so that we get same sequence of boards
        srand(g.number);
    }
    else
    {
        // seed PRNG with current time so that we get any sequence of boards
        srand(time(NULL));

        // choose a random n in [1, max]
        g.number = rand() % max + 1;
    }

    // start up ncurses
    if (!startup())
    {
        fprintf(stderr, "Error starting up ncurses!\n");
        return 5;
    }

    // register handler for SIGWINCH (SIGnal WINdow CHanged)
    signal(SIGWINCH, (void (*)(int)) handle_signal);

    // start the first game
    if (!restart_game())
    {
        shutdown();
        fprintf(stderr, "Could not load board from disk!\n");
        return 6;
    }
    redraw_all();

    // let the user play!
    int ch;

    int previous[3] = {10};
    do
    {
        // refresh the screen
        refresh();

        // get user's input
        ch = getch();

        // capitalize input to simplify cases
        ch = toupper(ch);

        // process user's input
        switch (ch)
        {
            // start a new game
            case 'N':
                g.number = rand() % max + 1;
                if (!restart_game())
                {
                    shutdown();
                    fprintf(stderr, "Could not load board from disk!\n");
                    return 6;
                }
                break;

            // restart current game
            case 'R':
                if (!restart_game())
                {
                    shutdown();
                    fprintf(stderr, "Could not load board from disk!\n");
                    return 6;
                }
                break;

            // let user manually redraw screen with ctrl-L
            case CTRL('l'):
                redraw_all();
                break;

            // Move cursor up
            case KEY_UP:
                if (g.y > 0)
                {
                    // Change position
                    g.y -= 1;
                    // Show cursor
                    show_cursor();
                }
                // Wrap to bottom
                else
                {
                    // Change position
                    g.y = 8;
                    // Show cursor
                    show_cursor();
                }
                break;

            // Move cursor down
            case KEY_DOWN:
                if (g.y < 8)
                {
                    // Change position
                    g.y += 1;
                    // Show cursor
                    show_cursor();
                }

                // Wrap cursor to top
                else
                {
                    // Change position
                    g.y = 0;
                    // Show cursor
                    show_cursor();
                }
                break;

            // Move cursor left
            case KEY_LEFT:
                if (g.x > 0)
                {
                    // Change position
                    g.x -= 1;
                    // Show cursor
                    show_cursor();
                }

                // Wrap around to end
                else
                {
                    // Change position
                    g.x = 8;
                    // Show cursor
                    show_cursor();
                }
                break;

             // Move cursor right
            case KEY_RIGHT:
                if (g.x < 8)
                {
                    // Change position
                    g.x += 1;
                    // Show cursor
                    show_cursor();
                }

                // Wrap around to beginning
                else
                {
                    // Change position
                    g.x = 0;
                    // Show cursor
                    show_cursor();
                }
                break;

                // Change blank to number
                case '1' ... '9':
                // If space is blank in original board
                if (g.initialBoard[g.y][g.x] == 0)
                {
                    // Saving moves as previous incase of undo
                    previous[0] = g.y;
                    previous[1] = g.x;
                    previous[2] = g.board[g.y][g.x];
                    // Change to entered number
                    g.board[g.y][g.x] = (ch) - 48;
                    // Print new board & show cursor
                    draw_numbers();
                    if (checkColumns(1) || checkRows(1) || checkSquares(1))
                    hide_banner();
                    show_cursor();
                }
                // See if game has been won
                wonGame();
                // See if there's an error
                warning();
                    break;

             // Delete an entered number
             case '0': case '.': case KEY_BACKSPACE: case KEY_DC:
                // If space is blank in original board
                if (g.initialBoard[g.y][g.x] == 0)
                {
                    // Save moves as previous incase of undo
                    previous[0] = g.y;
                    previous[1] = g.x;
                    previous[2] = g.board[g.y][g.x];
                    // Change to blank
                    g.board[g.y][g.x] = 0;
                    // Draw numbers & show cursor
                    draw_numbers();
                    show_cursor();
                }
                // Display warning for possible error
                warning();
                break;

            // Undo move
            case 'U': case CTRL('Z'):
                // If possible to undo
                if (previous[0] != 10 && !wonGame())
                {
                    // Call previous move
                    g.board[previous[0]][previous[1]] = previous[2];
                    // Redraw board
                    draw_numbers();
                    // Check numbers
                    if (checkColumns(1) || checkRows(1) || checkSquares(1))
                    hide_banner();
                    show_cursor();
                    // Display warning for possible error
                    warning();
                }
                break;

        }

        // log input (and board's state) if any was received this iteration
        if (ch != ERR)
            log_move(ch);
    }
    while (ch != 'Q');

    // shut down ncurses
    shutdown();

    // tidy up the screen (using ANSI escape sequences)
    printf("\033[2J");
    printf("\033[%d;%dH", 0, 0);

    // that's all folks
    printf("\nkthxbai!\n\n");
    return 0;
}

/*
 * Draw's the game's board.
 */

void
draw_grid(void)
{
    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // determine where top-left corner of board belongs
    g.top = maxy/2 - 7;
    g.left = maxx/2 - 30;

    // enable color if possible
    if (has_colors())
        attron(COLOR_PAIR(PAIR_GRID));

    // print grid
    for (int i = 0 ; i < 3 ; ++i )
    {
        mvaddstr(g.top + 0 + 4 * i, g.left, "+-------+-------+-------+");
        mvaddstr(g.top + 1 + 4 * i, g.left, "|       |       |       |");
        mvaddstr(g.top + 2 + 4 * i, g.left, "|       |       |       |");
        mvaddstr(g.top + 3 + 4 * i, g.left, "|       |       |       |");
    }
    mvaddstr(g.top + 4 * 3, g.left, "+-------+-------+-------+" );

    // remind user of level and #
    char reminder[maxx+1];
    sprintf(reminder, "   playing %s #%d", g.level, g.number);
    mvaddstr(g.top + 14, g.left + 25 - strlen(reminder), reminder);

    // disable color if possible
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_GRID));
}


/*
 * Draws game's borders.
 */

void
draw_borders(void)
{
    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // enable color if possible (else b&w highlighting)
    if (has_colors())
    {
        attron(A_PROTECT);
        attron(COLOR_PAIR(PAIR_BORDER));
    }
    else
        attron(A_REVERSE);

    // draw borders
    for (int i = 0; i < maxx; i++)
    {
        mvaddch(0, i, ' ');
        mvaddch(maxy-1, i, ' ');
    }

    // draw header
    char header[maxx+1];
    sprintf(header, "%s by %s", TITLE, AUTHOR);
    mvaddstr(0, (maxx - strlen(header)) / 2, header);

    // draw footer
    mvaddstr(maxy-1, 1, "[N]ew Game   [R]estart Game");
    mvaddstr(maxy-1, maxx-13, "[Q]uit Game");

    // disable color if possible (else b&w highlighting)
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_BORDER));
    else
        attroff(A_REVERSE);
}


/*
 * Draws game's logo.  Must be called after draw_grid has been
 * called at least once.
 */

void
draw_logo(void)
{
    // determine top-left coordinates of logo
    int top = g.top + 2;
    int left = g.left + 30;

    // enable color if possible
    if (has_colors())
        attron(COLOR_PAIR(PAIR_LOGO));

    // draw logo
    mvaddstr(top + 0, left, "               _       _          ");
    mvaddstr(top + 1, left, "              | |     | |         ");
    mvaddstr(top + 2, left, " ___ _   _  __| | ___ | | ___   _ ");
    mvaddstr(top + 3, left, "/ __| | | |/ _` |/ _ \\| |/ / | | |");
    mvaddstr(top + 4, left, "\\__ \\ |_| | (_| | (_) |   <| |_| |");
    mvaddstr(top + 5, left, "|___/\\__,_|\\__,_|\\___/|_|\\_\\\\__,_|");

    // sign logo
    char signature[3+strlen(AUTHOR)+1];
    sprintf(signature, "by %s", AUTHOR);
    mvaddstr(top + 7, left + 35 - strlen(signature) - 1, signature);

    // disable color if possible
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_LOGO));
}

// Checks columns for duplicates
bool
checkColumns(int check)
{
    // Capture every number in every column
    int col[9];
    for (int i = 0; i < 9; i++)
    {
        for(int j = 0; j < 9; j++)
        {
            col[j] = g.board[j][i];
            for(int k = 0; k < j; k++)
            {
                // Duplicate found
                if(check == 1 && col[k] == col[j] && col[k] != 0)
                return false;

                // Duplicate found for wonGame
                else if (check != 1 && col[k] == col[j])
                return false;
            }
        }
    }
    // No duplicates found
    return true;
}

// Check rows for duplicates
bool
checkRows(int check)
{
    // Captures every number in every row
    int row[9];
    for (int i = 0; i < 9; i++)
    {
        for(int j = 0; j < 9; j++)
        {
            row[j] = g.board[i][j];
            for(int k = 0; k < j; k++)
            {
                // Duplicate found
                if(check == 1 && row[k] == row[j] && row[k] != 0)
                return false;

                // Duplicate found for gameWon
                else if (check != 1 && row[k] == row[j])
                return false;
            }
        }
    }
    return true;
}

// Check squares for duplicates
bool
checkSquares(int check)
{
    /** a = initial y axis
    b = init x axis
    i = y axis
    j = x axis
    k = position in square **/

    int square[10];

    for(int a = 0; a < 7; a = a + 3)
    {
        for(int b = 0; b < 7; b = b + 3)
        {
            // Clear array
            memset(&square[0], 0, sizeof(square));

            // Capture all numbers
            for (int i = a; i < a + 3; i++)
            {
                for(int j = b; j < b + 3; j++)
                {
                    // Duplicate found
                    if (check == 1 && g.board[i][j] != 0 && square[g.board[i][j]] == 1)
                        return false;

                    // Duplicate found for gameWon
                    if (check != 1 && ( g.board[i][j] == 0 || square[g.board[i][j]] == 1))
                        return false;

                    square[g.board[i][j]] = 1;
                }
            }
        }
    }
    return true;
}

// If game is won, show banner & prevent changes
bool
wonGame(void)
{
    // Check if won & show banner
    if(checkColumns(0) && checkRows(0) && checkSquares(0))
    {
        show_banner("Congrats, you won!");

        // Prevents user from changing numbers
        for (int i = 0; i < 9; i++)
           {
               for(int j = 0; j < 9; j++)
                  {
                      g.initialBoard[j][i] = 9;
                  }
           }
         return true;
    }

    else
    return false;
}

/*
 * Draw's game's numbers.  Must be called after draw_grid has been
 * called at least once.
 */

void
draw_numbers(void)
{
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            // Give all numbers the digits color
            if (has_colors()) {
                attron(COLOR_PAIR(PAIR_DIGITS));
                // Change color of number to yellow if the number is given
                if (g.initialBoard[i][j] == g.board[i][j]) {
                     attron(COLOR_PAIR(PAIR_INIT));
                }
                // Change all numbers to green if game is won
                if (wonGame()) {
                    attron(COLOR_PAIR(PAIR_WON));
                }
            // determine char
            char c = (g.board[i][j] == 0) ? '.' : g.board[i][j] + '0';
            mvaddch(g.top + i + 1 + i/3, g.left + 2 + 2*(j + j/3), c);
            refresh();
            attroff(COLOR_PAIR(PAIR_INIT));
            }
        }
    }
}


/*
 * Designed to handles signals (e.g., SIGWINCH).
 */

void
handle_signal(int signum)
{
    // handle a change in the window (i.e., a resizing)
    if (signum == SIGWINCH)
        redraw_all();

    // re-register myself so this signal gets handled in future too
    signal(signum, (void (*)(int)) handle_signal);
}


/*
 * Hides banner.
 */

void
hide_banner(void)
{
    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // overwrite banner with spaces
    for (int i = 0; i < maxx; i++)
        mvaddch(g.top + 16, i, ' ');
}


/*
 * Loads current board from disk, returning true iff successful.
 */

bool
load_board(void)
{
    // open file with boards of specified level
    char filename[strlen(g.level) + 5];
    sprintf(filename, "%s.bin", g.level);
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        return false;

    // determine file's size
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    // ensure file is of expected size
    if (size % (81 * INTSIZE) != 0)
    {
        fclose(fp);
        return false;
    }

    // compute offset of specified board
    int offset = ((g.number - 1) * 81 * INTSIZE);

    // seek to specified board
    fseek(fp, offset, SEEK_SET);

    // read board into memory
    if (fread(g.board, 81 * INTSIZE, 1, fp) != 1)
    {
        fclose(fp);
        return false;
    }

    // w00t
    fclose(fp);

    // Store a copy of initial board
    // For every coloumn
    for (int i = 0; i < 9; i++)
    {
        // For every row
        for(int j = 0; j < 9; j++)
        {
            // Store each number in initialBoard
            g.initialBoard[j][i] = g.board[j][i];
        }
    }

    return true;
}


/*
 * Logs input and board's state to log.txt to facilitate automated tests.
 */

void
log_move(int ch)
{
    // open log
    FILE *fp = fopen("log.txt", "a");
    if (fp == NULL)
        return;

    // log input
    fprintf(fp, "%d\n", ch);

    // log board
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
            fprintf(fp, "%d", g.board[i][j]);
        fprintf(fp, "\n");
    }

    // that's it
    fclose(fp);
}


/*
 * (Re)draws everything on the screen.
 */

void
redraw_all(void)
{
    // reset ncurses
    endwin();
    refresh();

    // clear screen
    clear();

    // re-draw everything
    draw_borders();
    draw_grid();
    draw_logo();
    draw_numbers();

    // show cursor
    show_cursor();
}


/*
 * (Re)starts current game, returning true iff succesful.
 */

bool
restart_game(void)
{
    // reload current game
    if (!load_board())
        return false;

    // redraw board
    draw_grid();
    draw_numbers();

    // get window's dimensions
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // move cursor to board's center
    g.y = g.x = 4;
    show_cursor();

    // remove log, if any
    remove("log.txt");

    // w00t
    return true;
}


/*
 * Shows cursor at (g.y, g.x).
 */

void
show_cursor(void)
{
    // restore cursor's location
    move(g.top + g.y + 1 + g.y/3, g.left + 2 + 2*(g.x + g.x/3));
}


/*
 * Shows a banner.  Must be called after show_grid has been
 * called at least once.
 */

void
show_banner(char *b)
{
    // enable color if possible
    if (has_colors())
        attron(COLOR_PAIR(PAIR_BANNER));

    // determine where top-left corner of board belongs
    mvaddstr(g.top + 16, g.left + 64 - strlen(b), b);

    // disable color if possible
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_BANNER));
}


/*
 * Shuts down ncurses.
 */

void
shutdown(void)
{
    endwin();
}

// Check if there is an issue with the current move
void
warning(void)
{
    if (!checkColumns(1))
    show_banner("You have a column problem");
    if (!checkRows(1))
    show_banner("You have a row problem");
    if (!checkSquares(1))
    show_banner("You have a square problem");
    if (!checkColumns(1) && !checkRows(1))
    show_banner("You have a column and a row problem");
    if (!checkColumns(1) && !checkSquares(1))
    show_banner("You have a column and a square problem");
    if (!checkRows(1) && !checkSquares(1))
    show_banner("You have a row problem a square problem");
    if (!checkRows(1) && !checkColumns(1) && !checkSquares(1))
    show_banner("You have a problem with the column, row, and square");
    show_cursor();
}


/*
 * Starts up ncurses.  Returns true iff successful.
 */

bool
startup(void)
{
    // initialize ncurses
    if (initscr() == NULL)
        return false;

    // prepare for color if possible
    if (has_colors())
    {
        // enable color
        if (start_color() == ERR || attron(A_PROTECT) == ERR)
        {
            endwin();
            return false;
        }

        // initialize pairs of colors
        if (init_pair(PAIR_BANNER, FG_BANNER, BG_BANNER) == ERR ||
            init_pair(PAIR_GRID, FG_GRID, BG_GRID) == ERR ||
            init_pair(PAIR_BORDER, FG_BORDER, BG_BORDER) == ERR ||
            init_pair(PAIR_LOGO, FG_LOGO, BG_LOGO) == ERR ||
            init_pair(PAIR_DIGITS, FG_DIGITS, BG_DIGITS) == ERR ||
            init_pair(PAIR_WON, FG_WON, BG_WON) == ERR ||
            init_pair(PAIR_INIT, FG_INIT, BG_INIT) == ERR)
        {
            endwin();
            return false;
        }
    }

    // don't echo keyboard input
    if (noecho() == ERR)
    {
        endwin();
        return false;
    }

    // disable line buffering and certain signals
    if (raw() == ERR)
    {
        endwin();
        return false;
    }

    // enable arrow keys
    if (keypad(stdscr, true) == ERR)
    {
        endwin();
        return false;
    }

    // wait 1000 ms at a time for input
    timeout(1000);

    // w00t
    return true;
}
