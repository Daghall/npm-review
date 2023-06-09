// vi: fdm=marker
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <curses.h>
#include <signal.h>
#include <functional>
#include <iterator>
#include <iterator>
#include <ncurses.h>
#include <regex>
#include <stdio.h>
#include <string>
#include <vector>
#include "npm-review.h"

using namespace std;

const char *INFO_STRING = {
#include "build/info"
};

const char *DEPENDENCIES_STRING = {
#include "build/dependencies"
};

// Debugging
FILE *debug_log = NULL;
#define debug(...)    if (debug_log != NULL) { \
  fprintf(debug_log, __VA_ARGS__); \
  fflush(debug_log); \
}
bool fake_http_requests = false;

// Constants
#define LIST_HEIGHT   (USHORT)(LINES - BOTTOM_BAR_HEIGHT)
#define LAST_LINE     (USHORT)(LINES - 1)
#define ctrl(c)       (c & 0x1f)

// Global variables
WINDOW *alternate_window = NULL;
WINDOW *package_window = NULL;
vector<PACKAGE> pkgs;
vector<PACKAGE> filtered_packages;
vector<string> alternate_rows;
unsigned short selected_package = 0;
short selected_alternate_row = 0;
enum alternate_modes alternate_mode;
bool show_sub_dependencies = false;

int main(int argc, const char *argv[])
{
  while (--argc > 0) {
    switch (argv[argc][1]) {
      case 'd':
        debug_log = fopen("./log", "w");
        break;
      case 'f':
        fake_http_requests = true;
        break;
    }
  }

  initialize();

  MAX_LENGTH max_length;
  max_length.name = 0;
  max_length.version = 0;

  read_packages(&max_length);

  // TODO: Handle resize
  // TODO: Sorting?
  // TODO: Cache stuff?
  const unsigned short package_size = (short) pkgs.size();
  const unsigned short number_of_packages = max(LIST_HEIGHT, package_size);
  package_window = newpad(number_of_packages, COLS);
  debug("Number of packages: %d\n", number_of_packages);

  unsigned short start_packages = 0;
  unsigned short start_alternate = 0;
  bool search_mode = false;
  string search_string = "";

  while (true) {
    unsigned short package_index = 0;
    string regex_parse_error;

    // Searching
    try {
      regex search_regex (search_string);
      filtered_packages.clear();
      copy_if(pkgs.begin(), pkgs.end(), back_inserter(filtered_packages), [&search_regex](PACKAGE &package) {
        return regex_search(package.name, search_regex);
      });
    } catch(const regex_error &e) {
      regex_parse_error = e.what();
    }

    const unsigned short number_of_filtered_packages = (USHORT) filtered_packages.size();
    const size_t package_number_width = number_width(package_size);

    if (selected_package > number_of_filtered_packages - 1) {
      selected_package = number_of_filtered_packages - 1;

      if (number_of_filtered_packages < LIST_HEIGHT) {
        start_packages = 0;
      }
    }

    werase(package_window);

    // Render packages
    for_each(filtered_packages.begin(), filtered_packages.end(), [&package_index, &max_length](PACKAGE &package) {
      wattron(package_window, COLOR_PAIR(COLOR_PACKAGE));
      if (package_index == selected_package) {
        wattron(package_window, COLOR_PAIR(COLOR_SELECTED_PACKAGE));
      }
      wprintw(package_window, " %-*s  %-*s%s \n", max_length.name, package.name.c_str(), max_length.version, package.version.c_str(), package.is_dev ? "  (DEV)" : "");
      ++package_index;
    });

    // Package scrolling
    if (selected_package == 0) {
      start_packages = 0;
    } else if (selected_package == number_of_filtered_packages - 1) {
      start_packages = max(number_of_filtered_packages - LIST_HEIGHT, 0);
    } else if (selected_package >= LIST_HEIGHT + start_packages) {
      ++start_packages;
    } else if (selected_package < start_packages) {
      --start_packages;
    }

    // Alternate scrolling
    const unsigned short alternate_length = max(LIST_HEIGHT, (USHORT) alternate_rows.size());

    if (selected_alternate_row <= 0) {
      start_alternate = 0;
    } else if (selected_alternate_row == alternate_length - 1) {
      start_alternate = alternate_length - LIST_HEIGHT;
    } else if (selected_alternate_row >= LIST_HEIGHT + start_alternate) {
      start_alternate = selected_alternate_row - LIST_HEIGHT + 1;
    } else if (start_alternate > selected_alternate_row) {
      start_alternate = selected_alternate_row;
    } else if (selected_alternate_row < start_alternate) {
      --start_alternate;
    }

    // Refresh windows
    debug("Refresh main... %d - %d | %d\n", selected_package, LIST_HEIGHT, start_packages);
    prefresh(package_window, start_packages, 0, 0, 0, LIST_HEIGHT - 1, COLS);
    if (alternate_window) {
      debug("Refresh alternate... %d - %d | %d\n", selected_alternate_row, LIST_HEIGHT, start_alternate);
      prefresh(alternate_window, start_alternate, 0, 0, COLS / 2 + 1, LIST_HEIGHT - 1 , COLS - 1);
    }

    // Render bottom bar
    attron(COLOR_PAIR(COLOR_INFO_BAR));
    // Clear and set background on the entire line
    mvprintw(LIST_HEIGHT, 0, "%*s", COLS, "");

    if (regex_parse_error.length() > 0) {
      mvprintw(LIST_HEIGHT, 0, " %s", regex_parse_error.c_str());
    } else if (filtered_packages.size() == 0) {
      mvprintw(LIST_HEIGHT, 0, " No matches");
    } else {
      if (search_string.length() > 0 ) {
        mvprintw(LIST_HEIGHT, 0, " %*d/%-*d (%d)", package_number_width, selected_package + 1, package_number_width, filtered_packages.size(), pkgs.size());
      } else {
        mvprintw(LIST_HEIGHT, 0, " %*d/%-*d", package_number_width, selected_package + 1, package_number_width, filtered_packages.size());
      }
    }

    if (alternate_window) {
      const unsigned short alternate_number_width = number_width(alternate_rows.size());
      mvprintw(LIST_HEIGHT, COLS / 2, "  %*d/%d", alternate_number_width, selected_alternate_row + 1, alternate_rows.size());
    }
    attroff(COLOR_PAIR(COLOR_INFO_BAR));

    if (search_mode) {
      move(LAST_LINE, search_string.length() + 1);
    } else {
      // Move to last line to make `npm install` render here
      move(LAST_LINE, 0);
    }

    const short character = getch();

    if (search_mode) {
      debug("Sending key '%c' (%#x) to search\n", character, character);
      switch (character) {
        case ctrl('c'):
        case KEY_ESC:
          search_mode = false;
          search_string = "";
          clear_message();
          hide_cursor();
          break;
        case ctrl('z'):
          search_mode = false;
          hide_cursor();
          raise(SIGTSTP);
        case '\n':
          search_mode = false;
          hide_cursor();
          break;
        case KEY_DELETE:
        case KEY_BACKSPACE:
        case '\b': {
          const short last_character = search_string.length() - 1;

          if (last_character >= 0) {
            search_string.erase(last_character, 1);
            clear_message();
            show_searchsting(search_string);
          } else {
            search_mode = false;
            clear_message();
            hide_cursor();
          }
          break;
        }
        default:
          if (is_printable(character)) {
            search_string += character;
            show_searchsting(search_string);
          }
      }
      debug("Searching for \"%s\"\n", search_string.c_str());
    } else if (alternate_window) {
      debug("Sending key '%d' (%#x) to alternate window\n", character, character);
      switch (character) {
        case ctrl('z'):
          raise(SIGTSTP);
          break;
        // TODO: Bail if no packages, or exit?
        case KEY_DOWN:
        case 'J':
          selected_package = (selected_package + 1) % filtered_packages.size();
          change_alternate_window();
          start_alternate = 0;
          break;
        case KEY_UP:
        case 'K':
          selected_package = (selected_package - 1 + filtered_packages.size()) % filtered_packages.size();
          change_alternate_window();
          start_alternate = 0;
          break;
        case 'j':
          selected_alternate_row = min(selected_alternate_row + 1, (int) alternate_rows.size() - 1);
          if (alternate_rows.at(selected_alternate_row) == "") {
            selected_alternate_row = min(selected_alternate_row + 1, (int) alternate_rows.size() - 1);
          }
          print_alternate(filtered_packages.at(selected_package));
          break;
        case 'k':
          selected_alternate_row = max(selected_alternate_row - 1, 0);
          if (alternate_rows.at(selected_alternate_row) == "") {
            selected_alternate_row = max(selected_alternate_row - 1, 0);
          }
          print_alternate(filtered_packages.at(selected_package));
          break;
        case 'h':
          if (alternate_mode == DEPENDENCIES) {
            if (show_sub_dependencies) {
              show_sub_dependencies = false;
              get_dependencies(filtered_packages.at(selected_package), false);
            } else {
              close_alternate_window();
            }
          }
          break;
        case 'l':
          if (alternate_mode == DEPENDENCIES && !show_sub_dependencies) {
            show_sub_dependencies = true;
            get_dependencies(filtered_packages.at(selected_package), false);
          }
          break;
        case 'q':
          close_alternate_window();
          break;
        case ctrl('c'):
        case 'Q':
          return exit();
        case '\n':
          if (alternate_mode == VERSION) {
            install_package(filtered_packages.at(selected_package), alternate_rows.at(selected_alternate_row));
          }
          break;
        case 'g':
          selected_alternate_row = 0;
          print_alternate(filtered_packages.at(selected_package));
          break;
        case 'G':
          selected_alternate_row = (int) alternate_rows.size() - 1;
          print_alternate(filtered_packages.at(selected_package));
          break;
      }
    } else { // Package window
      debug("Sending key '%c' (%#x) to main window\n", character, character);
      switch (character) {
        case ERR:
          nodelay(stdscr, false); // Make `getch` block
          break;
        case ctrl('z'):
          raise(SIGTSTP);
          break;
        case KEY_DOWN:
        case 'j':
        case 'J':
          selected_package = (selected_package + 1) % filtered_packages.size();
          break;
        case KEY_UP:
        case 'k':
        case 'K':
          selected_package = (selected_package - 1 + filtered_packages.size()) % filtered_packages.size();
          break;
          break;
        case 'i':
          // TODO: Bounds check?
          alternate_mode = INFO;
          get_info(filtered_packages.at(selected_package));
          break;
        case 'I':
        case 'l':
          // TODO: Bounds check?
          alternate_mode = DEPENDENCIES;
          get_dependencies(filtered_packages.at(selected_package));
          break;
        case '\n':
          // TODO: Bounds check?
          alternate_mode = VERSION;
          get_versions(filtered_packages.at(selected_package));
          break;
        case 'D': {
          // TODO: Bounds check?
          PACKAGE package = filtered_packages.at(selected_package);
          uninstall_package(package);
          break;
        }
        case ctrl('c'):
        case 'q':
        case 'Q':
          return exit();
        case 'g':
          selected_package = 0;
          break;
        case 'G':
          selected_package = number_of_packages - 1;
          break;
        case '/':
          search_mode = true;
          clear_message();
          show_searchsting(search_string);
          show_cursor();
          break;
      }
    }
  }
}

void initialize() {
  debug("Initializing\n");
  setlocale(LC_ALL, "");  // Support UTF-8 characters
  initscr();              // Init ncurses screen
  keypad(stdscr, true);   // Activate extend keyboard detection
  ESCDELAY = 0;           // Remove delay of ESC, since no escape sequences are expected
  raw();                  // Handle signals manually, via `getch`
  noecho();               // Do not echo key-presses
  nodelay(stdscr, true);  // Make `getch` non-blocking
  start_color();
  use_default_colors();
  assume_default_colors(COLOR_WHITE, COLOR_DEFAULT);
  hide_cursor();

  init_pair(COLOR_SELECTED_PACKAGE, COLOR_BLACK, COLOR_GREEN);
  init_pair(COLOR_PACKAGE, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_OLD_VERSION, COLOR_RED, COLOR_DEFAULT);
  init_pair(COLOR_CURRENT_VERSION, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_INFO_BAR, COLOR_BLACK, COLOR_BLUE);
  debug("Initialized. Columns: %d. Lines: %d\n", COLS, LINES);
}

vector<string> split_string(string package_string)
{
  char buffer[1024];
  vector<string> result = vector<string>();

  strcpy(buffer, package_string.c_str());
  char *str = strtok(buffer, " ");

  while (str != NULL) {
    result.push_back(string(str));
    str = strtok(NULL, " ");
  }

  return result;
}

void read_packages(MAX_LENGTH *max_length)
{
  debug("read_packages\n");
  string command = "for dep in .dependencies .devDependencies; do jq $dep' | keys[] as $key | \"\\($key) \\(.[$key] | sub(\"[~^]\"; \"\")) '$dep'\"' -r 2> /dev/null < package.json; done";
  vector<string> packages = shell_command(command);
  pkgs.clear();

  for_each(packages.begin(), packages.end(), [&max_length](string &package) {
    const vector<string> strings = split_string(package);
    string name = strings.at(0);
    string version = strings.at(1);
    string origin = strings.at(2);
    pkgs.push_back({
      .name = name,
      .version = version,
      .is_dev = origin == ".devDependencies"
    });

    if (max_length && name.length() > max_length->name) {
      max_length->name = name.length();
    }
    if (max_length && version.length() > max_length->version) {
      max_length->version = version.length();
    }
  });
}

void get_versions(PACKAGE package)
{
  const char* package_name = package.name.c_str();

  // TODO: Break out and clean up:
  mvprintw(0, COLS / 2 - 1, "  Loading...                 "); // TODO: Pretty up
  for (int i = 1; i < LIST_HEIGHT; ++i) {
    mvprintw(i, COLS / 2, "                               "); // TODO: Pretty up
  }
  refresh();

  selected_alternate_row = -1;

  char command[1024];
  // TODO: Break out to script file:
  snprintf(command, 1024, "npm info %s versions --json | jq 'if (type == \"array\") then reverse | .[] else . end' -r", package_name);

  if (fake_http_requests) { // {{{1
    alternate_rows.clear();
    alternate_rows.push_back("25.0.0");
    alternate_rows.push_back("24.0.0");
    alternate_rows.push_back("23.0.0");
    alternate_rows.push_back("22.0.0");
    alternate_rows.push_back("21.0.0");
    alternate_rows.push_back("20.0.0");
    alternate_rows.push_back("19.0.0");
    alternate_rows.push_back("18.0.0");
    alternate_rows.push_back("17.0.0");
    alternate_rows.push_back("16.0.0");
    alternate_rows.push_back("15.0.0");
    alternate_rows.push_back("14.0.0");
    alternate_rows.push_back("13.0.0");
    alternate_rows.push_back("12.0.0");
    alternate_rows.push_back("12.0.0");
    alternate_rows.push_back("11.0.0");
    alternate_rows.push_back("10.0.0");
    alternate_rows.push_back("9.0.0");
    alternate_rows.push_back("8.0.0");
    alternate_rows.push_back("7.0.0");
    alternate_rows.push_back("6.0.0");
    alternate_rows.push_back("5.3.5");
    alternate_rows.push_back("4.0.0");
    alternate_rows.push_back("3.0.0");
    alternate_rows.push_back("2.0.0");
    alternate_rows.push_back("1.0.0");
  } else { // }}}
    alternate_rows = shell_command(command);
  }
  print_alternate(package);
}

void get_dependencies(PACKAGE package, bool init) {
  // TODO: Add loading screen
  string package_name = escape_slashes(package.name);
  char command[1024];
  snprintf(command, 1024, DEPENDENCIES_STRING, package_name.c_str(), show_sub_dependencies ? "^$" : "[│ ] ");
  string selected;

  if (!init) {
    selected = find_dependency_root();
  }

  if (fake_http_requests) { // {{{1
    alternate_rows.clear();
    if (!show_sub_dependencies) {
      alternate_rows.push_back("┬ express-handlebars    5.3.5");
      alternate_rows.push_back("├─┬ glob                7.2.0");
      alternate_rows.push_back("├── graceful-fs         4.2.10");
      alternate_rows.push_back("└─┬ handlebars          4.7.7");
    } else {
      alternate_rows.push_back("┬ express-handlebars    5.3.5");
      alternate_rows.push_back("├─┬ glob                7.2.0");
      alternate_rows.push_back("│ ├── fs.realpath       1.0.0");
      alternate_rows.push_back("│ ├─┬ inflight          1.0.6");
      alternate_rows.push_back("│ │ ├── once            1.4.0");
      alternate_rows.push_back("│ │ └── wrappy          1.0.2");
      alternate_rows.push_back("│ ├── inherits          2.0.4");
      alternate_rows.push_back("│ ├── minimatch         3.1.2");
      alternate_rows.push_back("│ ├─┬ once              1.4.0");
      alternate_rows.push_back("│ │ └── wrappy          1.0.2");
      alternate_rows.push_back("│ └── path-is-absolute  1.0.1");
      alternate_rows.push_back("├── graceful-fs         4.2.10");
      alternate_rows.push_back("└─┬ handlebars          4.7.7");
      alternate_rows.push_back("  ├── minimist          1.2.6");
      alternate_rows.push_back("  ├── neo-async         2.6.2");
      alternate_rows.push_back("  ├── source-map        0.6.1");
      alternate_rows.push_back("  ├── uglify-js         3.15.3");
      alternate_rows.push_back("  └── wordwrap          1.0.0");
    }
  } else { // }}}
    alternate_rows = shell_command(command);
  }

  selected_alternate_row = 0;

  if (!init) {
    select_dependency_node(selected);
  }

  print_alternate(package);
}

void get_info(PACKAGE package)
{
  // TODO: Add loading screen
  string package_name = escape_slashes(package.name);
  const char*  package_version = package.version.c_str();
  char command[1024];
  snprintf(command, 1024, INFO_STRING, package_name.c_str(), package_version, package_version);

  if (fake_http_requests) { // {{{1
    alternate_rows.clear();
    alternate_rows.push_back("express-handlebars | BSD-3-Clause");
    alternate_rows.push_back("A Handlebars view engine for Express which doesn't suck.");
    alternate_rows.push_back("");
    alternate_rows.push_back("CURRENT");
    alternate_rows.push_back("5.3.5 (2021-11-13)");
    alternate_rows.push_back("");
    alternate_rows.push_back("LATEST");
    alternate_rows.push_back("7.0.7 (2023-04-15)");
    alternate_rows.push_back("");
    alternate_rows.push_back("DEPENDENCIES");
    alternate_rows.push_back("– glob");
    alternate_rows.push_back("– graceful-fs");
    alternate_rows.push_back("– handlebars");
    alternate_rows.push_back("");
    alternate_rows.push_back("HOMEPAGE");
    alternate_rows.push_back("https://github.com/express-handlebars/express-handlebars");
    alternate_rows.push_back("");
    alternate_rows.push_back("AUTHOR");
    alternate_rows.push_back("Eric Ferraiuolo <eferraiuolo@gmail.com> (http://ericf.me/)");
    alternate_rows.push_back("");
    alternate_rows.push_back("KEYWORDS");
    alternate_rows.push_back("express, express3, handlebars, view, layout, partials, templates");
  } else { // }}}
    alternate_rows = shell_command(command);
  }
  selected_alternate_row = 0;
  print_alternate(package);
}


void install_package(PACKAGE package, const string new_version)
{
  // Move to last line to make `npm install` render here
  move(LAST_LINE, 0);

  char command[1024];
  snprintf(command, 1024, "npm install %s@%s --silent", package.name.c_str(), new_version.c_str());
  int exit_code = sync_shell_command(command, [](char* line) {
    debug("NPM INSTALL: %s\n", line);
  });

  hide_cursor();

  if (exit_code == OK) {
    read_packages(NULL);
    selected_alternate_row = -1;
    PACKAGE p = filtered_packages.at(selected_package);
    p.version = new_version;
    print_alternate(p);
  } else {
    // TODO: Handle error
    debug("Install failed\n");
  }
}

void uninstall_package(PACKAGE package)
{
  if (!confirm("Uninstall " + package.name + "?")) {
    return;
  }

  char command[1024];

  snprintf(command, 1024, "npm uninstall %s --silent", package.name.c_str());
  int exit_code = sync_shell_command(command, [](char* line) {
    debug("NPM UNINSTALL: %s\n", line);
  });

  hide_cursor();

  if (exit_code == OK) {
    debug("Package uninstalled\n");
    show_message("Uninstalled " + package.name);
    read_packages(NULL);
  } else {
    // TODO: Handle error
    debug("Uninstall failed\n");
  }
}

void show_message(string message, const USHORT color)
{
  debug("Showing message: \"%s\", color: %d\n", message.c_str(), color);
  if (color != COLOR_DEFAULT) {
    attron(COLOR_PAIR(color));
  }
  mvprintw(LAST_LINE, 0, "%s", message.c_str());
  if (color != COLOR_DEFAULT) {
    attroff(COLOR_PAIR(color));
  }
}

void show_searchsting(string search_string)
{
  show_message("/" + search_string);
}

void clear_message()
{
  move(LAST_LINE, 0);
  clrtoeol();
}

void close_alternate_window() {
  wclear(alternate_window);
  delwin(alternate_window);
  alternate_window = NULL;
}

bool confirm(string message)
{
  clear_message();
  string confirm_message = message + " [y/n]";
  show_message(confirm_message, COLOR_PACKAGE);
  move(LAST_LINE, confirm_message.length());
  show_cursor();

  const short answer = getch();

  clear_message();
  hide_cursor();

  if (tolower(answer) == 'y') {
    return true;
  }

  return false;
}

// TODO: Color unmet deps?
// TODO: Handle pad size when rows exceed one row
void print_alternate(PACKAGE package)
{
  string package_version = package.version;
  if (alternate_window) {
    wclear(alternate_window);
    delwin(alternate_window);
  }
  int alternate_length = max(LIST_HEIGHT, (USHORT) alternate_rows.size());
  debug("Number of alternate items: %d\n", alternate_length);
  alternate_window = newpad(alternate_length, COLS / 2);
  size_t index = 0;
  for_each(alternate_rows.begin(), alternate_rows.end(), [package_version, &index](string &version) {
    if (version == package_version) {
      wattron(alternate_window, COLOR_PAIR(COLOR_CURRENT_VERSION));
      if (selected_alternate_row < 0) {
        selected_alternate_row = index;
      }
    }
    if (selected_alternate_row == index) {
      wattron(alternate_window, A_STANDOUT);
    }
    wprintw(alternate_window," %s \n", version.c_str());
    if (version == package_version) {
      wattroff(alternate_window, A_STANDOUT);
      wattron(alternate_window, COLOR_PAIR(COLOR_OLD_VERSION));
    }
    wattroff(alternate_window, A_STANDOUT);
    ++index;
  });
}

void change_alternate_window() {
  switch (alternate_mode) {
  case VERSION:
    get_versions(filtered_packages.at(selected_package));
    break;
  case DEPENDENCIES:
    get_dependencies(filtered_packages.at(selected_package));
    break;
  case INFO:
    get_info(filtered_packages.at(selected_package));
    break;
  }
}

string find_dependency_root()
{
  if (alternate_rows.size() == 0) return "";

  regex root ("^(└|├)");
  while (selected_alternate_row > 0 && !regex_search(alternate_rows.at(selected_alternate_row), root)) {
    --selected_alternate_row;
  }

  string match = alternate_rows.at(selected_alternate_row);
  int end = match.find("  ");

  return match.substr(0, end);
}

void select_dependency_node(string &selected) {
  int size = alternate_rows.size();
  int selected_length = selected.length();
  string row;

  while (true) {
    row = alternate_rows.at(selected_alternate_row);
    if (row.compare(0, selected_length, selected) == 0) break;
    if (selected_alternate_row >= size) break;

    ++selected_alternate_row;
  }
}

vector<string> shell_command(const string command)
{
  vector<string> result;
  char buffer[1024];
  FILE *output = popen(command.c_str(), "r");

  debug("Executing \"%s\"\n", command.c_str());

  while (fgets(buffer, sizeof(buffer), output) != NULL) {
    size_t last = strlen(buffer) - 1;
    buffer[last] = '\0';
    result.push_back(buffer);
  }

  pclose(output);
  return result;
}

int sync_shell_command(const string command, std::function<void(char*)> callback)
{
  char buffer[1024];
  FILE *output = popen(command.c_str(), "r");
  setvbuf(output, buffer, _IOLBF, sizeof(buffer));

  while (fgets(buffer, sizeof(buffer), output) != NULL) {
    size_t last = strlen(buffer) - 1;
    buffer[last] = '\0';
    callback(buffer);
  }

  return pclose(output);
}

const unsigned short number_width(unsigned short number)
{
  unsigned short rest = number;
  unsigned short i = 0;

  while (rest > 0) {
    rest /= 10;
    ++i;
  }

  return i;
}

string escape_slashes(string str) {
  regex slash ("/");
  return regex_replace(str, slash, "\\/");
}

bool is_printable(char character)
{
  return character >= 0x20 && character < 0x7F;
}

void show_cursor()
{
  curs_set(1);
}

void hide_cursor()
{
  show_cursor(); // Explicitly show it to avoid rendering bug after `npm {un,}install`
  curs_set(0);
}

int exit()
{
  fclose(debug_log);
  if (alternate_window) {
    delwin(alternate_window);
  }
  return endwin();
}
