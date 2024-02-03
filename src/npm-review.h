#ifndef _NPM_REVIEW_H
#define _NPM_REVIEW_H

#include <map>
#include <string>
#include <vector>
#include "ncurses.h"
#include "types.h"

using namespace std;

enum alternate_modes {
  VERSION,
  DEPENDENCIES,
  INFO,
  VERSION_CHECK,
};

#define CACHE_TYPE string, vector<string>
typedef pair<CACHE_TYPE> cache_item;
typedef map<CACHE_TYPE> cache_type;


// Constants

const USHORT COMMAND_SIZE = 1024;

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
void read_packages(MAX_LENGTH *foo);
vector<string> shell_command(const string);
void print_alternate(PACKAGE *package = nullptr);
vector<string> get_versions(PACKAGE package);
void print_versions(PACKAGE package, int alternate_row = -1);
map<CACHE_TYPE>* get_cache();
vector<string> get_from_cache(string package_name, char* command);
void get_dependencies(PACKAGE package, bool init = true);
void get_info(PACKAGE package);
void get_all_versions();
void change_alternate_window();
void print_package_bar();
void skip_empty_rows(USHORT &start_alternate, short adjustment);
string find_dependency_root();
void select_dependency_node(string &selected);
int sync_shell_command(const string command, std::function<void(char*)> callback);
void init_alternate_window(bool show_loading_message = false);
void install_package(PACKAGE package, const string new_version);
void uninstall_package(PACKAGE package);
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
