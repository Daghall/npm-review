#ifndef _SEARCH_H
#define _SEARCH_H

#include <ncurses.h>
#include <string>
#include <vector>

using namespace std;

// TODO: Add history
typedef struct {
  string string;
  bool reverse;
} SEARCH;


class Search {
  bool search_mode;
  WINDOW **alternate_window;

  public:
    SEARCH package;
    SEARCH alternate;

    Search(WINDOW **alternate_window);

    vector<short> search_hits;
    void initialize_search(bool reverse = false);
    string *get_active_string();
    SEARCH *get_active_search();
    void show_search_string();
    bool is_search_mode();
    void search_hit_prev(short &selected_alternate_row);
    void search_hit_next(short &selected_alternate_row);
    void enable();
    void disable();
    void clear();
};

#endif // _SEARCH_H
