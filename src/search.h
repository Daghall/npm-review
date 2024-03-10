#ifndef _SEARCH_H
#define _SEARCH_H

#include "types.h"
#include <fstream>
#include <ncurses.h>
#include <string>
#include <vector>

using namespace std;

const string HISTORY_FILE_NAME = "~/.npm-review-history";
const USHORT HISTORY_MAX_SIZE = 50;

typedef struct {
  string string;
  bool reverse;
} SEARCH;


class Search {
  bool search_mode;
  WINDOW **alternate_window;
  vector<string> history;
  fstream history_file;
  size_t history_index;
  string filter_string;

  void open_history_file(const int flags);
  void read_history();
  void history_save();
  USHORT history_get(bool next = false);
  SEARCH *get_active_search();

  public:
    SEARCH package;
    SEARCH alternate;

    Search(WINDOW **alternate_window);

    vector<short> search_hits;
    USHORT initialize_search(bool reverse = false);
    string *get_active_string();
    void show_search_string();
    void update_search_string(USHORT position, char character = '\0');
    bool is_search_mode();
    void enable();
    void disable();
    void finilize();
    void clear();
    void search_hit_prev(short &selected_alternate_row);
    void search_hit_next(short &selected_alternate_row);
    USHORT history_prev();
    USHORT history_next();
};

#endif // _SEARCH_H
