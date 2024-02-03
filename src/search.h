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
    void initialize_search(bool reverse = false);
    string *get_active_string();
    SEARCH *get_active_search();
    void show_search_string();
    bool is_search_mode();
    void enable();
    void disable();
    void clear();
};

#endif // _SEARCH_H
