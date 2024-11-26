#include <fstream>
#include <vector>
#include "ncurses.h"
#include "debug.h"
#include "search.h"
#include "string.h"
#include "types.h"

Search::Search(WINDOW **alt_win) {
  alternate_window = alt_win;
  search_mode = false;
  history_index = 0;
  filter_string = "";

  read_history();
}

void Search::open_history_file(const int flags)
{
  const char *home = getenv("HOME");
  string history_file_path = HISTORY_FILE_NAME;
  history_file.open(history_file_path.replace(0, 1, home), flags);
}

void Search::read_history()
{
  debug("Reading search history file: %s\n", HISTORY_FILE_NAME.c_str());
  open_history_file(fstream::in);

  while (history_file.good()) {
    char line[256];
    history_file.getline(line, 256);

    if (strlen(line) > 0) {
      history.push_back(line);
    }
  }

  history_file.close();
}

void Search::history_save()
{
  string active_string = *get_active_string();
  debug("Saving to search history: %s\n", active_string.c_str());
  vector<string>::iterator str_pos = find(history.begin(), history.end(), active_string);

  if (str_pos != history.end()) {
    history.erase(str_pos);
  }

  history.insert(history.begin(), active_string);

  if (history.size() > HISTORY_MAX_SIZE) {
    history.resize(HISTORY_MAX_SIZE);
  }

  open_history_file(fstream::out | fstream::trunc);

  for (string &str : history) {
    history_file << str << endl;
  }

  history_file.close();
}

USHORT Search::history_get(bool next)
{
  SEARCH *as = get_active_search();
  vector<string> result;
  result.push_back(filter_string);

  copy_if(history.begin(), history.end(), back_inserter(result), [this](string item) {
    return starts_with(item, filter_string);
  });

  if (next) {
    if (history_index > 0) {
      --history_index;
    }
  } else {
    if (history_index < result.size() - 1) {
      ++history_index;
    }
  }

  debug("Using search history item #%d/%d\n", history_index + 1, result.size());
  as->string = result.at(history_index);
  show_search_string();

  return as->string.length() + 1;
}


/*
 * PUBLIC METHODS
 */

SEARCH* Search::get_active_search()
{
  return *alternate_window != nullptr
    ? &alternate
    : &package;
}

bool Search::is_search_mode()
{
  return search_mode;
}

void Search::enable()
{
  search_mode = true;
}

void Search::disable()
{
  search_mode = false;
}

void Search::finilize()
{
  disable();
  history_save();
}

void Search::clear()
{
  disable();
  SEARCH &s = *alternate_window ? alternate : package;
  s.string = "";
}

USHORT Search::initialize_search(bool reverse)
{
  SEARCH *as = get_active_search();
  as->string.clear();
  as->reverse = reverse;
  search_mode = true;
  clear_message();
  show_search_string();
  show_cursor();
  filter_string.clear();

  return 1;
}

string* Search::get_active_string() {
  return *alternate_window != nullptr
    ? &alternate.string
    : &package.string;
}

void Search::show_search_string()
{
  SEARCH *search = get_active_search();
  show_message((search->reverse ? "?" : "/") + search->string);
}

void Search::update_search_string(USHORT position, char character)
{
  string *str = get_active_string();

  if (character == '\0') {
    str->erase(position - 1, 1);
  } else {
    str->insert(position - 1, 1, character);
  }

  filter_string = *str;
  history_index = 0;
}

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
  show_error_message("Search hit TOP, continuing at BOTTOM");
  selected_alternate_row = last;
}

void Search::search_hit_next(short &selected_alternate_row)
{
  debug("Search for previous hit\n");
  if (search_hits.size() == 0) {
    debug("No search hits\n");
    return;
  }
  vector<short>::iterator it (search_hits.begin());

  while (++it != search_hits.end()) {
    if (*it > selected_alternate_row) {
      selected_alternate_row = *it;
      return;
    }
  }
  const short first = *search_hits.begin();
  debug("Search wrapped to row %d\n", first);
  show_error_message("Search hit BOTTOM, continuing at TOP");
  selected_alternate_row = *search_hits.begin();
}

USHORT Search::history_prev()
{
  return history_get(false);
}

USHORT Search::history_next()
{
  return history_get(true);
}
