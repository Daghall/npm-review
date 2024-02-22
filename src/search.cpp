#include <vector>
#include "ncurses.h"
#include "debug.h"
#include "search.h"

using namespace std;

Search::Search(WINDOW **alt_win) {
  alternate_window = alt_win;
  search_mode = false;
  vector<short> search_hits;
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

// TODO: Return new index instead?
void Search::search_hit_prev(short &selected_alternate_row)
{
  debug("Search for previous hit\n");
  if (search_hits.size() == 0) {
    debug("No search hits\n");
    return;
  }

  vector<short>::iterator it (search_hits.end());

  while (it-- != search_hits.cbegin()) {
    if (*it < selected_alternate_row) {
      selected_alternate_row = *it;
      debug("Search hit at row %d\n", selected_alternate_row);
      return;
    }
  }

  const short last = *(search_hits.end() - 1);
  debug("Search wrapped to row %d\n", last);
  show_error_message("search hit TOP, continuing at BOTTOM");
  selected_alternate_row = last;
}

void Search::search_hit_next(short &selected_alternate_row)
{
  if (search_hits.size() == 0) return;
  vector<short>::iterator it (search_hits.begin());

  while (++it != search_hits.end()) {
    if (*it > selected_alternate_row) {
      selected_alternate_row = *it;
      return;
    }
  }
  show_error_message("search hit BOTTOM, continuing at TOP");
  selected_alternate_row = *search_hits.begin();
}
