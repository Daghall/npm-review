#ifndef _TYPES_H
#define _TYPES_H

#include <map>
#include <string>
#include <vector>
#include <curses.h>

using namespace std;

// Stringify, used in the Makefile to build include files
#define STR(x) #x

typedef unsigned short USHORT;

#define CACHE_TYPE string, vector<string>
typedef pair<CACHE_TYPE> cache_item;
typedef map<CACHE_TYPE> cache_type;

#define VERSION_TYPE string, string
typedef pair<VERSION_TYPE> version_item;
typedef unordered_map<VERSION_TYPE> version_type;

typedef struct {
  string name;
  string alias;
  string version;
  bool is_dev;
  string original_version;
} PACKAGE;

typedef struct {
  USHORT name;
  USHORT search;
  USHORT version;
} MAX_LENGTH;

enum alternate_modes {
  VERSION,
  DEPENDENCIES,
  INFO,
  VERSION_CHECK,
};

enum main_modes {
  INSTALL,
  PACKAGES,
  FILTERED,
};

class Search; // Forward declaration, to avoid circular dependencies

typedef struct {
  WINDOW *alternate_window;
  WINDOW *package_window;
  vector<PACKAGE> pkgs;
  vector<PACKAGE> filtered_packages;
  vector<string> alternate_rows;
  USHORT selected_package;
  short selected_alternate_row;
  USHORT start_alternate;
  USHORT start_packages;
  enum alternate_modes alternate_mode;
  enum main_modes main_mode;
  bool show_sub_dependencies;
  bool refresh_packages;
  bool list_versions;
  string regex_parse_error;
  Search *searching;
  USHORT cursor_position;
} VIEW;

#endif // _TYPES_H
