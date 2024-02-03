#include <vector>
#include "ncurses.h"
#include "debug.h"
#include "search.h"

using namespace std;

Search::Search(WINDOW **alt_win) {
  alternate_window = alt_win;
  search_mode = false;
}

bool Search::is_search_mode() {
  return search_mode;
}

void Search::enable() {
  search_mode = true;
}

void Search::disable() {
  search_mode = false;
}

void Search::clear() {
  disable();
  SEARCH &s = *alternate_window ? alternate : package;
  s.string = "";
}

void Search::initialize_search(bool reverse)
{
  SEARCH *s = get_active_search();
  s->reverse = reverse;
  search_mode = true;
  clear_message();
  show_search_string();
  show_cursor();
}

string* Search::get_active_string() {
  return *alternate_window != nullptr
    ? &alternate.string
    : &package.string;
}

SEARCH* Search::get_active_search()
{
  return *alternate_window != nullptr
    ? &alternate
    : &package;
}

void Search::show_search_string()
{
  SEARCH *search = get_active_search();
  show_message((search->reverse ? "?" : "/") + search->string);
}
