#ifndef _NCURSES_H
#define _NCURSES_H

#include "types.h"
#include <curses.h>

#define LIST_HEIGHT (USHORT)(LINES - BOTTOM_BAR_HEIGHT)
#define LAST_LINE   (USHORT)(LINES - 1)

const USHORT COLOR_DEFAULT = -1;
const USHORT COLOR_SELECTED_PACKAGE = 1;
const USHORT COLOR_PACKAGE = 2;
const USHORT COLOR_ERROR = 3;
const USHORT COLOR_CURRENT_VERSION = 4;
const USHORT COLOR_INFO_BAR = 5;
const USHORT COLOR_SEARCH = 6;
const USHORT COLOR_EDITED_PACKAGE = 7;

const USHORT KEY_ESC = 0x1B;
const USHORT KEY_DELETE = 0x7F;

const USHORT BOTTOM_BAR_HEIGHT = 2;
const USHORT KEY_SEQUENCE_DISTANCE = 10;

void initialize();
void show_cursor();
void hide_cursor();
void getch_blocking_mode(bool should_block);
void close_alternate_window(WINDOW **alternate_window);
void render_alternate_window_border();
bool confirm(string message);
bool is_message_shown();
void show_message(string message, const USHORT color = COLOR_DEFAULT);
void show_error_message(string message);
void show_key_sequence(string message);
void clear_message();
void clear_key_sequence();

#endif // _NCURSES_H
