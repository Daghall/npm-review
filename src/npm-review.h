#ifndef _NPM_REVIEW_H
#define _NPM_REVIEW_H

#include "ncurses.h"
#include "types.h"

using namespace std;

// Stringify, used in the Makefile to build include files
#define STR(x) #x

// Functions

// Simple "hash" function to convert a string of [a-z] characters into an int,
// to be used in a switch. Long strings will overflow!
// Remove 0x60 (making 'a' = 1, 'z' = 26).
// Shift left with base 26.
constexpr int H(const char* str, int sum = 0) {
  return *str
    ? H(str + 1, (*str - 0x60) + sum * 26)
    : sum;
}

void initialize();
void print_alternate(PACKAGE *package = nullptr);
void print_versions(PACKAGE package, int alternate_row = -1);
void get_all_versions();
void change_alternate_window();
void render_package_bar();
void render_alternate_bar();
void skip_empty_rows(USHORT &start_alternate, short adjustment);
string find_dependency_root();
void select_dependency_node(string &selected);
void init_alternate_window(bool show_loading_message = false);
void package_window_up();
void package_window_down();
void alternate_window_up();
void alternate_window_down();
void render_alternate_window_border();
void show_cursor();
void hide_cursor();
void getch_blocking_mode(bool should_block);
int exit();

#endif // _NPM_REVIEW_H
