#include <clocale>
#include <curses.h>
#include "debug.h"
#include "ncurses.h"

bool message_shown = false;

void initialize()
{
  debug("Initializing\n");
  setlocale(LC_ALL, "");  // Support UTF-8 characters
  initscr();              // Init ncurses screen
  keypad(stdscr, true);   // Activate extend keyboard detection
  ESCDELAY = 0;           // Remove delay of ESC, since no escape sequences are expected
  raw();                  // Handle signals manually, via `getch`
  noecho();               // Do not echo key-presses
  getch_blocking_mode(false);
  start_color();
  use_default_colors();
  assume_default_colors(COLOR_WHITE, COLOR_DEFAULT);
  hide_cursor();

  init_pair(COLOR_SELECTED_PACKAGE, COLOR_BLACK, COLOR_GREEN);
  init_pair(COLOR_PACKAGE, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_ERROR, COLOR_RED, COLOR_DEFAULT);
  init_pair(COLOR_CURRENT_VERSION, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_INFO_BAR, COLOR_DEFAULT, COLOR_BLUE);
  init_pair(COLOR_SEARCH, COLOR_DEFAULT, COLOR_YELLOW);
  debug("Initialized. Columns: %d. Lines: %d\n", COLS, LINES);
}

void show_cursor()
{
  curs_set(1);
}

void hide_cursor()
{
  show_cursor(); // Explicitly show it to avoid rendering bug after `npm {un,}install`
  curs_set(0);
}

void getch_blocking_mode(bool should_block)
{
  nodelay(stdscr, !should_block);
}

void render_alternate_window_border()
{
  attron(COLOR_PAIR(COLOR_INFO_BAR));
  for (int i = 0; i < LIST_HEIGHT; ++i) {
    mvprintw(i, COLS / 2 - 1, "│");
  }
  attroff(COLOR_PAIR(COLOR_INFO_BAR));
}

void close_alternate_window(WINDOW **alternate_window)
{
  clear_message();
  wclear(*alternate_window);
  delwin(*alternate_window);
  *alternate_window = NULL;

  for (int i = 0; i < LIST_HEIGHT; ++i) {
    move(i, COLS / 2 - 1);
    clrtoeol();
  }
  refresh();
}

bool confirm(string message)
{
  clear_message();
  string confirm_message = message + " [y/n]";
  show_message(confirm_message, COLOR_PACKAGE);
  move(LAST_LINE, confirm_message.length());
  show_cursor();

  const short answer = getch();

  clear_message();
  hide_cursor();

  if (tolower(answer) == 'y') {
    return true;
  }

  return false;
}

bool is_message_shown()
{
  return message_shown;
}

void show_message(string message, const USHORT color)
{
  clear_message();
  message_shown = true;
  debug("Showing message: \"%s\", color: %d\n", message.c_str(), color);
  if (color != COLOR_DEFAULT) {
    attron(COLOR_PAIR(color));
  }
  mvprintw(LAST_LINE, 0, "%s", message.c_str());
  if (color != COLOR_DEFAULT) {
    attroff(COLOR_PAIR(color));
  }
  refresh();
}

void show_error_message(string message)
{
  show_message(message, COLOR_ERROR);
}

void show_key_sequence(string message)
{
  mvprintw(LAST_LINE, COLS - KEY_SEQUENCE_DISTANCE, "%s", message.c_str());
}

void clear_message()
{
  move(LAST_LINE, 0);
  clrtoeol();
  message_shown = false;
}

void clear_key_sequence()
{
  move(LAST_LINE, COLS - KEY_SEQUENCE_DISTANCE);
  clrtoeol();
}
