#ifndef _DEBUG_H
#define _DEBUG_H

#include "types.h"
#include "ncurses.h"
#include <cstdarg>
#include <curses.h>
#include <stdio.h>


void init_logging();
void debug(const char *format, ...);
void debug_key(const char character, const char *window_name);
void terminate_logging();
char translate_character(const wchar_t character);
void dump_screen(VIEW &view);

#endif // _DEBUG_H
