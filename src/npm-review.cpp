// vi: fdm=marker
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <curses.h>
#include <map>
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
#include "debug.h"

using namespace std;

const char *INFO_STRING = {
#include "../build/info"
};

const char *DEPENDENCIES_STRING = {
#include "../build/dependencies"
};

bool fake_http_requests = false;

// "Constants"
#define LIST_HEIGHT   (USHORT)(LINES - BOTTOM_BAR_HEIGHT)
#define LAST_LINE     (USHORT)(LINES - 1)
#define ctrl(c)       (c & 0x1f)

// Global variables
WINDOW *alternate_window = NULL;
WINDOW *package_window = NULL;
vector<PACKAGE> pkgs;
vector<PACKAGE> filtered_packages;
vector<string> alternate_rows;

USHORT selected_package = 0;
short selected_alternate_row = 0;

USHORT start_alternate = 0;
USHORT start_packages = 0;

enum alternate_modes alternate_mode;
bool show_sub_dependencies = false;
bool refresh_packages = true;

bool list_versions = false;
bool search_mode = false;
bool message_shown = false;
SEARCH search_packages;
SEARCH search_alternate;
string regex_parse_error;


// Caching
cache_type dependency_cache;
cache_type info_cache;
cache_type version_cache;

int main(int argc, const char *argv[])
{
  while (--argc > 0) {
    switch (argv[argc][0]) {
      case '-':
        switch (argv[argc][1]) {
          case 'd':
            init_logging();
            break;
          case 'f':
            fake_http_requests = true;
            break;
          case 'p':
          {
            pkgs.push_back({
              .name = argv[argc + 1],
            });
            break;
          }
          case 'V':
            list_versions = true;
            break;
        }
        break;
      case '/':
        search_packages.string = argv[argc];
        search_packages.string.erase(0, 1);
        break;
    }
  }

  initialize();

  MAX_LENGTH max_length;
  max_length.name = 0;
  max_length.version = 0;

  if (pkgs.size() == 1) {
    show_message("Fetching versions for " + pkgs.at(0).name + "...");
    alternate_mode = VERSION;
    // TODO: Handle invalid package name
    print_versions(pkgs.at(0), 0);
  } else {
    read_packages(&max_length);
  }

  // TODO: Move "utility" functions to separate files
  // TODO: Sorting?
  // TODO: Cache "views"
  // TODO: Help screen
  // TODO: Timeout on network requests?
  // TODO: "Undo" – install original version
  // TODO: Clear cache and reload from network/disk on ctrl-l
  // TODO: gx – open URL?
  // TODO: Make `gj` work in dependency and info mode

  const USHORT package_size = (short) pkgs.size();
  const USHORT number_of_packages = max(LIST_HEIGHT, package_size);
  package_window = newpad(number_of_packages, 1024);
  debug("Number of packages: %d\n", number_of_packages);

  string key_sequence = "";

  while (true) {
    USHORT package_index = 0;

    // Filtering packages
    try {
      regex search_regex (search_packages.string);
      regex_parse_error = "";
      filtered_packages.clear();
      copy_if(pkgs.begin(), pkgs.end(), back_inserter(filtered_packages), [&search_regex](PACKAGE &package) {
        return regex_search(package.name, search_regex);
      });
    } catch (const regex_error &e) {
      regex_parse_error = e.what();
    }

    const USHORT number_of_filtered_packages = (USHORT) filtered_packages.size();

    if (selected_package > number_of_filtered_packages - 1) {
      selected_package = number_of_filtered_packages - 1;

      if (number_of_filtered_packages < LIST_HEIGHT) {
        start_packages = 0;
      }
    }

    wclear(package_window);

    const string search_string = get_active_search()->string;
    if (search_string != "" && !message_shown) {
      show_search_string();
    }

    if (key_sequence != "") {
      show_key_sequence(key_sequence);
    }

    // Render packages
    for_each(filtered_packages.begin(), filtered_packages.end(), [&package_index, &max_length](PACKAGE &package) {
      wattron(package_window, COLOR_PAIR(COLOR_PACKAGE));
      if (package_index == selected_package) {
        wattron(package_window, COLOR_PAIR(COLOR_SELECTED_PACKAGE));
      }
      wprintw(package_window, " %-*s  %-*s%-*s \n", max_length.name, package.name.c_str(), max_length.version, package.version.c_str(), COLS, package.is_dev ? "  (DEV)" : "");
      ++package_index;
    });

    // Package scrolling
    if (selected_package <= 0) {
      start_packages = 0;
    } else if (selected_package >= LIST_HEIGHT + start_packages) {
      start_packages = selected_package - LIST_HEIGHT + 1;
    } else if (start_packages > selected_package) {
      start_packages = selected_package;
    } else if (selected_package < start_packages) {
      --start_packages;
    }

    // Alternate scrolling
    if (selected_alternate_row <= 0) {
      start_alternate = 0;
    } else if (selected_alternate_row >= LIST_HEIGHT + start_alternate) {
      start_alternate = selected_alternate_row - LIST_HEIGHT + 1;
    } else if (start_alternate > selected_alternate_row) {
      start_alternate = selected_alternate_row;
    } else if (selected_alternate_row < start_alternate) {
      --start_alternate;
    }

    // Render bottom bar
    attron(COLOR_PAIR(COLOR_INFO_BAR));
    mvprintw(LIST_HEIGHT, 0, "%*s", COLS, ""); // Clear and set background on the entire line
    // TODO: Use clrtoeol instead?
    attron(COLOR_PAIR(COLOR_INFO_BAR));

    print_package_bar();

    if (alternate_window) {
      attron(COLOR_PAIR(COLOR_INFO_BAR));
      char x_of_y[32];
      snprintf(x_of_y, 32, "%d/%zu", selected_alternate_row + 1, alternate_rows.size());
      const char* alternate_mode_string = alternate_mode_to_string();
      mvprintw(LIST_HEIGHT, COLS / 2 - 1, "│ [%s] %*s", alternate_mode_string, COLS / 2 - (5 + strlen(alternate_mode_string)), x_of_y);
      attroff(COLOR_PAIR(COLOR_INFO_BAR));
    }

    // Refresh package window
    if (refresh_packages) {
      debug("Refreshing main... %d - %d | %d\n", selected_package, LIST_HEIGHT, start_packages);

      // Clear the window to remove residual lines when refreshing pad smaller
      // than the full width
      for (int i = start_packages - selected_package; i < LIST_HEIGHT; ++i) {
        move(i, 0);
        clrtoeol();
      }
      refresh();

      if (alternate_window) {
        // Only refresh the part that is visible
        prefresh(package_window, start_packages, 0, 0, 0, LIST_HEIGHT - 1, COLS / 2 - 2);
        render_alternate_window_border();
      } else {
        prefresh(package_window, start_packages, 0, 0, 0, LIST_HEIGHT - 1, COLS - 1);
      }

      refresh_packages = false;
    }

    // Refresh alternate window
    if (alternate_window) {
      debug("Refreshing alternate... %d - %d | %d\n", selected_alternate_row, LIST_HEIGHT, start_alternate);

      // Clear the window to remove residual lines when refreshing pad smaller
      // than the full width
      for (int i = start_alternate - selected_alternate_row; i < LIST_HEIGHT; ++i) {
        move(i, COLS / 2);
        clrtoeol();
      }

      refresh();
      prefresh(alternate_window, start_alternate, 0, 0, COLS / 2, LIST_HEIGHT - 1 , COLS - 1);
    }

    if (search_mode) {
      move(LAST_LINE, get_active_search()->string.length() + 1);
    }

    if (list_versions) {
      get_all_versions();

      getch_blocking_mode(false);
      if (list_versions && getch() == ctrl('c')) {
        list_versions = false;
        show_message("Aborted version check", COLOR_ERROR);
        getch_blocking_mode(true);
        print_alternate(&pkgs.at(selected_package));
        continue;
      }
    }

    const short character = getch();

    if (character == KEY_RESIZE) {
      clear();
      // TODO: Warn about too small screen? 44 seems to be minimum width
      debug("Resizing... Columns: %d, Lines: %d\n", COLS, LINES);
      refresh_packages = true;
      continue;
    } else if (character == ERR) {
      getch_blocking_mode(true);
      continue;
    }

    if (key_sequence.length() > 0) {
      key_sequence += character;
      debug("Key sequence: %s (%d)\n", key_sequence.c_str(), H(key_sequence.c_str()));

      if (alternate_window) {
        switch (H(key_sequence.c_str())) {
          case H("gg"):
            selected_alternate_row = 0;
            print_alternate();

            if (alternate_mode == VERSION_CHECK) {
              selected_package = selected_alternate_row;
              refresh_packages = true;
            }
            break;
          case H("gc"):
            {
              if (alternate_mode != VERSION) {
                debug("Key sequence ignored, inapplicable mode\n");
                break;
              }

              vector<string>::iterator current = find(alternate_rows.begin(), alternate_rows.end(), filtered_packages.at(selected_package).version);
              selected_alternate_row = distance(current, alternate_rows.begin());
              print_alternate();
              break;
            }
          case H("gj"):
            {
              if (alternate_mode != VERSION) {
                debug("Key sequence interpreted as 'j'\n");
                alternate_window_down();
                break;
              }

              const int major_version = atoi(alternate_rows.at(selected_alternate_row).c_str());
              vector<string>::iterator next_major = find_if(alternate_rows.begin() + selected_alternate_row, alternate_rows.end(), [&major_version](string item) {
                  return atoi(item.c_str()) != major_version;
                  });

              if (next_major == alternate_rows.end()) {
                selected_alternate_row = alternate_rows.size() - 1;
                show_error_message("Hit bottom, no more versions");
              } else {
                selected_alternate_row = distance(alternate_rows.begin(), next_major);
              }

              print_alternate();
              break;
            }
          case H("gk"):
            {
              if (alternate_mode != VERSION) {
                debug("Key sequence interpreted as 'k'\n");
                alternate_window_up();
                break;
              }
              const int major_version = atoi(alternate_rows.at(selected_alternate_row).c_str());
              vector<string>::reverse_iterator previous_major = find_if(alternate_rows.rbegin() + alternate_rows.size() - selected_alternate_row, alternate_rows.rend(), [&major_version](string item) {
                  return atoi(item.c_str()) != major_version;
                  });

              if (previous_major == alternate_rows.rend()) {
                selected_alternate_row = 0;
                show_error_message("Hit top, no more versions");
              } else {
                selected_alternate_row = distance(previous_major, alternate_rows.rend() - 1);
              }

              print_alternate();
              break;
            }
          case H("zt"):
            start_alternate = selected_alternate_row;
            break;
          case H("zz"):
            start_alternate = max(0, selected_alternate_row - LIST_HEIGHT / 2);
            break;
          case H("zb"):
            start_alternate = max(0, selected_alternate_row - LIST_HEIGHT);
            break;
          default:
            debug("Unrecognized sequence (%d)\n", H(key_sequence.c_str()));
        }
      } else { // Package window
        switch (H(key_sequence.c_str())) {
          case H("gg"):
            selected_package = 0;
            refresh_packages = true;
            break;
          case H("gj"):
            package_window_down();
            break;
          case H("gk"):
            package_window_up();
            break;
          case H("zt"):
            start_packages = selected_package;
            refresh_packages = true;
            break;
          case H("zz"):
            start_packages = max(0, selected_package - LIST_HEIGHT / 2);
            refresh_packages = true;
            break;
          case H("zb"):
            start_packages = max(0, selected_package - LIST_HEIGHT);
            refresh_packages = true;
            break;
          default:
            debug("Unrecognized sequence\n");
        }
      }

      key_sequence= "";
      clear_key_sequence();
      continue;
    }

    if (search_mode) {
      debug_key(character, "search");
      refresh_packages = true;
      SEARCH *search = get_active_search();
      string &search_string = search->string;

      switch (character) {
        case ctrl('c'):
        case KEY_ESC:
          search_mode = false;
          search_string = "";
          clear_message();
          hide_cursor();
          if (alternate_window) {
            print_alternate();
          }
          break;
        case ctrl('z'):
          search_mode = false;
          hide_cursor();
          raise(SIGTSTP);
        case '\n':
          search_mode = false;
          if (search_string == "") {
            show_error_message("Ignoring empty pattern");
          }
          hide_cursor();
          break;
        case KEY_DELETE:
        case KEY_BACKSPACE:
        case '\b': {
          const short last_character_position = search_string.length() - 1;

          if (last_character_position >= 0) {
            search_string.erase(last_character_position, 1);
            clear_message();
            show_search_string();
            if (alternate_window) {
              print_alternate();
            }
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
            show_search_string();
            if (alternate_window) {
              print_alternate();
            }
          }
      }
      debug("Searching for \"%s\"\n", get_active_search()->string.c_str());
    } else if (alternate_window) {
      debug_key(character, "alternate window");
      clear_message();

      switch (character) {
        case ctrl('z'):
          raise(SIGTSTP);
          break;
        case KEY_DOWN:
        case 'J':
          package_window_down();
          change_alternate_window();
          if (alternate_mode != VERSION_CHECK) {
            start_alternate = 0;
          }
          break;
        case KEY_UP:
        case 'K':
          package_window_up();
          change_alternate_window();
          if (alternate_mode != VERSION_CHECK) {
            start_alternate = 0;
          }
          break;
        case 'j':
          alternate_window_down();

          if (alternate_mode == VERSION_CHECK) {
            selected_package = selected_alternate_row;
            refresh_packages = true;
          }
          break;
        case 'k':
          alternate_window_up();

          if (alternate_mode == VERSION_CHECK) {
            selected_package = selected_alternate_row;
            refresh_packages = true;
          }
          break;
        case ctrl('e'):
          if (alternate_mode == VERSION_CHECK) continue;

          start_alternate = min((size_t) start_alternate + 1, alternate_rows.size() - 1);

            if (start_alternate > selected_alternate_row ) {
              ++selected_alternate_row;
              skip_empty_rows(start_alternate, 1);
            }
          break;
        case ctrl('y'):
          if (alternate_mode == VERSION_CHECK) continue;

          if (start_alternate > 0) {
            --start_alternate;

            if (start_alternate + LIST_HEIGHT == selected_alternate_row) {
              --selected_alternate_row;
              skip_empty_rows(start_alternate, -1);
            }
          }
          break;
        case ctrl('d'):
          start_alternate = min((size_t) start_alternate + LIST_HEIGHT / 2, alternate_rows.size() - 1);
          if (start_alternate > selected_alternate_row) {
            selected_alternate_row = min((size_t) start_alternate, alternate_rows.size());
          }

          if (alternate_mode == VERSION_CHECK) {
            start_packages = start_alternate;
            selected_package = selected_alternate_row;
            refresh_packages = true;
          }

          print_alternate();
          break;
        case ctrl('u'):
          start_alternate = max(start_alternate - LIST_HEIGHT / 2, 0);
          if (start_alternate < selected_alternate_row - LIST_HEIGHT) {
            selected_alternate_row = start_alternate + LIST_HEIGHT - 1;
          }
          if (alternate_mode == VERSION_CHECK) {
            start_packages = start_alternate;
            selected_package = selected_alternate_row;
            refresh_packages = true;
          }
          print_alternate();
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
          switch (alternate_mode) {
            case VERSION:
            install_package(filtered_packages.at(selected_package), alternate_rows.at(selected_alternate_row));
              break;
            case DEPENDENCIES:
            case INFO:
              break;
            case VERSION_CHECK:
              alternate_mode = VERSION;
              print_versions(filtered_packages.at(selected_package));
              break;
          }
          break;
        case 'g':
        case 'z':
          key_sequence = character;
          show_key_sequence(key_sequence);
          break;
        case 'G':
          selected_alternate_row = (int) alternate_rows.size() - 1;
          print_alternate();

          if (alternate_mode == VERSION_CHECK) {
            selected_package = selected_alternate_row;
            refresh_packages = true;
          }
          break;
        case '?':
          initialize_search(&search_alternate, true);
          break;
        case '/':
          initialize_search(&search_alternate);
          break;
      }
    } else { // Package window
      debug_key(character, "package window");
      clear_message();

      switch (character) {
        case ctrl('z'):
          raise(SIGTSTP);
          break;
        case KEY_DOWN:
        case 'j':
        case 'J':
          package_window_down();
          break;
        case KEY_UP:
        case 'k':
        case 'K':
          package_window_up();
          break;
          break;
        case 'i':
          if (filtered_packages.size() == 0) continue;
          alternate_mode = INFO;
          get_info(filtered_packages.at(selected_package));
          break;
        case 'I':
        case 'l':
          if (filtered_packages.size() == 0) continue;
          alternate_mode = DEPENDENCIES;
          get_dependencies(filtered_packages.at(selected_package));
          break;
        case '\n':
          if (filtered_packages.size() == 0) continue;
          alternate_mode = VERSION;
          print_versions(filtered_packages.at(selected_package));
          break;
        case 'D': {
          if (filtered_packages.size() == 0) continue;
          PACKAGE package = filtered_packages.at(selected_package);
          uninstall_package(package);
          break;
        }
        case ctrl('e'):
          ++start_packages;

          if (start_packages > selected_package) {
            selected_package = start_packages;
          }
          refresh_packages = true;
          break;
        case ctrl('y'):
          start_packages = max(0, start_packages - 1);

          if (start_packages == selected_package - LIST_HEIGHT) {
            selected_package = start_packages + LIST_HEIGHT - 1;
          }

          refresh_packages = true;
          break;
        case ctrl('d'):
          start_packages = min((size_t) start_packages + LIST_HEIGHT / 2, pkgs.size() - 1);

          if (start_packages > selected_package) {
            selected_package = min((size_t) start_packages, pkgs.size());
          }
          refresh_packages = true;
          break;
        case ctrl('u'):
          start_packages = max(start_packages - LIST_HEIGHT / 2, 0);

          if (start_packages < selected_package - LIST_HEIGHT) {
            selected_package = start_packages + LIST_HEIGHT - 1;
          }
          refresh_packages = true;
          break;
        case ctrl('c'):
        case 'q':
        case 'Q':
          return exit();
        case 'z':
        case 'g':
          key_sequence = character;
          show_key_sequence(key_sequence);
          break;
        case 'G':
          selected_package = number_of_packages - 1;
          refresh_packages = true;
          break;
        case '?':
          initialize_search(&search_packages, true);
          break;
        case '/':
          initialize_search(&search_packages);
          break;
        case 'V':
          selected_package = 0;
          selected_alternate_row = 0;
          refresh_packages = true;
          list_versions = true;
          break;
      }
    }
  }
}

void initialize()
{
  debug("Initializing\n");
  setlocale(LC_ALL, "");  // Support UTF-8 characters
  initscr();              // Init ncurses screen
  keypad(stdscr, true);   // Activate extend keyboard detection
  ESCDELAY = 0;           // Remove delay of ESC, since no escape sequences are expected
  raw();                  // Handle signals manually, via `getch`
  noecho();               // Do not echo key-presses
  getch_blocking_mode(false);
  start_color();
  use_default_colors();
  assume_default_colors(COLOR_WHITE, COLOR_DEFAULT);
  hide_cursor();

  init_pair(COLOR_SELECTED_PACKAGE, COLOR_BLACK, COLOR_GREEN);
  init_pair(COLOR_PACKAGE, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_ERROR, COLOR_RED, COLOR_DEFAULT);
  init_pair(COLOR_CURRENT_VERSION, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_INFO_BAR, COLOR_DEFAULT, COLOR_BLUE);
  init_pair(COLOR_SEARCH, COLOR_DEFAULT, COLOR_YELLOW);
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
  debug("Reading packages\n");
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

vector<string> get_versions(PACKAGE package)
{
  if (fake_http_requests) { // {{{1
    render_alternate_window_border();
    vector<string> fake_response = vector<string>();
    fake_response.push_back("5.1.1");
    fake_response.push_back("5.1.0");
    fake_response.push_back("5.0.0");
    fake_response.push_back("4.2.0");
    fake_response.push_back("4.1.3");
    fake_response.push_back("4.1.2");
    fake_response.push_back("4.1.1");
    fake_response.push_back("4.1.0");
    fake_response.push_back("4.0.0");
    fake_response.push_back("3.7.0");
    fake_response.push_back("3.6.0");
    fake_response.push_back("3.5.0");
    fake_response.push_back("3.4.0");
    fake_response.push_back("3.3.0");
    fake_response.push_back("3.2.0");
    fake_response.push_back("3.1.0");
    fake_response.push_back("3.0.0");
    fake_response.push_back("2.4.0");
    fake_response.push_back("2.3.0");
    fake_response.push_back("2.2.0");
    fake_response.push_back("2.1.0");
    fake_response.push_back("2.0.1");
    fake_response.push_back("2.0.0");
    fake_response.push_back("1.2.2");
    fake_response.push_back("1.2.1");
    fake_response.push_back("1.2.0");
    fake_response.push_back("1.0.0");
    return fake_response;
  } else { // }}}
    char command[COMMAND_SIZE];
    // TODO: Break out to script file:
    snprintf(command, COMMAND_SIZE, "npm info %s versions --json | jq 'if (type == \"array\") then reverse | .[] else . end' -r", package.name.c_str());
    return get_from_cache(package.name, command);
  }
}

void print_versions(PACKAGE package, int alternate_row)
{
  selected_alternate_row = alternate_row;
  alternate_rows = get_versions(package);
  print_alternate(&package);
}

cache_type* get_cache()
{
  switch (alternate_mode) {
    case DEPENDENCIES:
      return &dependency_cache;
        break;
    case INFO:
      return &info_cache;
    case VERSION:
    case VERSION_CHECK:
      return &version_cache;
        break;
  }
}

vector<string> get_from_cache(string package_name, char* command)
{
  cache_type *cache = get_cache();

  if (!cache) return vector<string>();

  cache_type::iterator cache_data = cache->find(package_name);

  if (cache_data != cache->end()) {
    debug("Cache HIT for \"%s\" (%s)\n", package_name.c_str(), alternate_mode_to_string());
    // TODO: This feels inappropriate here. Clean it up
    init_alternate_window();
  } else {
    debug("Cache MISS for \"%s\" (%s)\n", package_name.c_str(), alternate_mode_to_string());
    // TODO: This feels inappropriate here. Clean it up
    if (alternate_mode != VERSION_CHECK) {
      init_alternate_window(true);
    }
    cache_data = cache->insert(cache->begin(), cache_item (package_name, shell_command(command)));
  }

  return cache_data->second;
}

void get_dependencies(PACKAGE package, bool init)
{
  string package_name = escape_slashes(package.name);
  string selected;

  if (!init) {
    selected = find_dependency_root();
  }

  if (fake_http_requests) { // {{{1
    render_alternate_window_border();
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
    char command[COMMAND_SIZE];
    snprintf(command, COMMAND_SIZE, DEPENDENCIES_STRING, package_name.c_str(), "^$");
    vector<string> dependency_data = get_from_cache(package_name, command);

    if (show_sub_dependencies) {
      alternate_rows = dependency_data;
    } else {
      regex sub_dependency_regex ("^(│| )");
      alternate_rows.clear();
      copy_if(dependency_data.begin(), dependency_data.end(), back_inserter(alternate_rows), [&sub_dependency_regex](string dependency) {
        return !regex_search(dependency, sub_dependency_regex);
      });
    }
  }

  selected_alternate_row = 0;
  select_dependency_node(selected);
  print_alternate(&package);
}

void get_info(PACKAGE package)
{
  string package_name = escape_slashes(package.name);
  const char*  package_version = package.version.c_str();

  if (fake_http_requests) { // {{{1
    render_alternate_window_border();
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
    char command[COMMAND_SIZE];
    snprintf(command, COMMAND_SIZE, INFO_STRING, package_name.c_str(), package_version, package_version);
    alternate_rows = get_from_cache(package_name, command);
  }
  selected_alternate_row = 0;
  print_alternate(&package);
}

void init_alternate_window(bool show_loading_message)
{
  // Clear window
  for (int i = 0; i < LIST_HEIGHT; ++i) {
    move(i, COLS / 2);
    clrtoeol();
  }

  render_alternate_window_border();

  attron(COLOR_PAIR(COLOR_INFO_BAR));
  mvprintw(LIST_HEIGHT, COLS / 2 - 1, "│");
  attroff(COLOR_PAIR(COLOR_INFO_BAR));
  refresh();

  if (show_loading_message) {
    attron(COLOR_PAIR(COLOR_SELECTED_PACKAGE));
    mvprintw(0, COLS - 13, " Loading... ");
    attroff(COLOR_PAIR(COLOR_SELECTED_PACKAGE));
    refresh();
  }
}

void install_package(PACKAGE package, const string new_version)
{
  // Move to last line to make `npm install` render here
  show_message("");

  for_each(dependency_cache.begin(), dependency_cache.end(), [](cache_item item) {
      debug("  key: %s\n", item.first.c_str());
  });

  // TODO: Handle names with slashes
  dependency_cache.erase(package.name);
  info_cache.erase(package.name);

  char command[COMMAND_SIZE];
  snprintf(command, COMMAND_SIZE, "npm install %s@%s --silent", package.name.c_str(), new_version.c_str());
  int exit_code = sync_shell_command(command, [](char* line) {
    debug("NPM INSTALL: %s\n", line);
  });

  hide_cursor();

  if (exit_code == OK) {
    read_packages(NULL);
    selected_alternate_row = -1;
    PACKAGE package = filtered_packages.at(selected_package);
    package.version = new_version;
    print_alternate(&package);
  } else {
    // TODO: Handle error
    debug("Install failed\n");
  }

  show_message("Installed \"" + package.name + "@" + new_version + "\"");
}

// This function is called once per package, in the main loop,
// to make canceling possible.
void get_all_versions()
{
  if (selected_package == 0) {
    init_alternate_window();
    alternate_rows.clear();
    alternate_mode = VERSION_CHECK;
    selected_alternate_row = 0;

    for (int i = filtered_packages.size(); i > 0; --i) {
      alternate_rows.push_back("Pending...");
    }
  }

  PACKAGE package = filtered_packages.at(selected_package);
  string message = "Checking versions for \"" + package.name + "\"";
  show_message(message.c_str());
  print_alternate(&package);
  prefresh(alternate_window, start_alternate, 0, 0, COLS / 2, LIST_HEIGHT - 1 , COLS - 1);
  refresh_packages = true;

  vector<string> versions = get_versions(package);
  string current_major = get_major(package.version);
  string latest_major = get_major(versions.at(0));

  vector<string>::iterator first_of_current = find_if(versions.begin(), versions.end(), [&current_major](string item) {
    return get_major(item) == current_major;
  });

  int versions_behind = distance(first_of_current, find(versions.begin(), versions.end(), package.version));
  debug("\"%s\": versions behind: %d – current major: %s, latest major: %s, latest version: %s\n", package.name.c_str(), versions_behind, current_major.c_str(), latest_major.c_str(), versions.at(0).c_str());

  string version_string = "";
  if (versions_behind > 0) {
    version_string += to_string(versions_behind);
    version_string += " new";
  } else {
    version_string += "     ✓";
  }

  string major_string = "";
  if (current_major != latest_major) {
    major_string += "– new major: ";
    major_string += latest_major;
  }
  char formatted_string[128];
  snprintf(formatted_string, 128, " %6s %-14s ", version_string.c_str(), major_string.c_str());

  alternate_rows.at(selected_package) = formatted_string;

  if (++selected_package >= filtered_packages.size()) {
    list_versions = false;
    clear_message();
    print_alternate(&pkgs.at(selected_package - 1));
  } else {
    selected_alternate_row = selected_package;
  }
}

string get_major(string semver)
{
  const size_t dot = semver.find(".");
  return semver.substr(0, dot);
}

void uninstall_package(PACKAGE package)
{
  if (!confirm("Uninstall " + package.name + "?")) {
    return;
  }

  char command[COMMAND_SIZE];
  snprintf(command, COMMAND_SIZE, "npm uninstall %s --silent", package.name.c_str());

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
    show_error_message("Uninstall failed");
  }
}

void show_key_sequence(string message)
{
  mvprintw(LAST_LINE, COLS - KEY_SEQUENCE_DISTANCE, "%s", message.c_str());
}

void show_message(string message, const USHORT color)
{
  clear_message();
  message_shown = true;
  debug("Showing message: \"%s\", color: %d\n", message.c_str(), color);
  if (color != COLOR_DEFAULT) {
    attron(COLOR_PAIR(color));
  }
  mvprintw(LAST_LINE, 0, "%s", message.c_str());
  if (color != COLOR_DEFAULT) {
    attroff(COLOR_PAIR(color));
  }
  refresh();
}

void show_error_message(string message)
{
  show_message(message, COLOR_ERROR);
}

void initialize_search(SEARCH *search, bool reverse)
{
  search->reverse = reverse;
  search_mode = true;
  clear_message();
  show_search_string();
  show_cursor();
}

SEARCH *get_active_search()
{
  return alternate_window
    ? &search_alternate
    : &search_packages;
}

void show_search_string()
{
  SEARCH *search = get_active_search();
  show_message((search->reverse ? "?" : "/") + search->string);
}

void clear_message()
{
  move(LAST_LINE, 0);
  clrtoeol();
  message_shown = false;
}

void clear_key_sequence()
{
  move(LAST_LINE, COLS - KEY_SEQUENCE_DISTANCE);
  clrtoeol();
}

void close_alternate_window()
{
  clear_message();
  wclear(alternate_window);
  delwin(alternate_window);
  alternate_window = NULL;

  for (int i = 0; i < LIST_HEIGHT; ++i) {
    move(i, COLS / 2 - 1);
    clrtoeol();
  }
  refresh();
  refresh_packages = true;
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
void print_alternate(PACKAGE *package)
{
  if (alternate_window) {
    wclear(alternate_window);
    delwin(alternate_window);
  }

  if (package == nullptr) {
    package = &filtered_packages.at(selected_package);
  }

  string package_version = package->version;
  const int alternate_length = max(LIST_HEIGHT, (USHORT) alternate_rows.size());
  size_t index = 0;
  alternate_window = newpad(alternate_length, 1024);

  debug("Number of alternate items/rows: %zu/%d\n", alternate_rows.size(), alternate_length);

  regex search_regex;
  try {
    // TODO: Show error message?
    search_regex = search_alternate.string;
  } catch (const regex_error &e) {
    debug("Regex error: %s\n", e.what());
  }

  USHORT color = COLOR_DEFAULT;

  for_each(alternate_rows.begin(), alternate_rows.end(), [package_version, &index, &search_regex, &color](const string &row) {
    if (row == package_version) {
      wattron(alternate_window, COLOR_PAIR(COLOR_CURRENT_VERSION));
      color = COLOR_CURRENT_VERSION;
      if (selected_alternate_row < 0) {
        selected_alternate_row = index;
      }
    }

    if (selected_alternate_row == index) {
      wattron(alternate_window, A_STANDOUT);
    }

    if (search_alternate.string != "") {
      smatch matches;
      string::const_iterator start = row.cbegin();
      size_t prev = 0;

      wprintw(alternate_window, " ");

      while (regex_search(start, row.end(), matches, search_regex, regex_constants::match_any)) {
        const string match = matches[0];
        const string rest (start, row.cend());
        size_t match_position = matches.position(0);

        if (match.length() == 0) break;

        wprintw(alternate_window, "%s", rest.substr(0, match_position).c_str());

        wattroff(alternate_window, A_STANDOUT); // Color fix
        wattron(alternate_window, COLOR_PAIR(COLOR_SEARCH));
        wprintw(alternate_window, "%s", match.c_str());
        wattroff(alternate_window, COLOR_PAIR(COLOR_SEARCH));
        if (selected_alternate_row == index) wattron(alternate_window, A_STANDOUT); // Color fix

        if (color != COLOR_DEFAULT) {
          wattron(alternate_window, COLOR_PAIR(color));
        }

        prev = match_position + match.length();
        start += prev;
      }

      const string rest (start - prev, row.cend());
      wprintw(alternate_window, "%-*s\n", 512 - prev, rest.substr(prev).c_str());
    } else {
      wprintw(alternate_window," %-*s \n", 512, row.c_str());
    }

    // TODO: Save hits for n/N navigation

    if (row == package_version) {
      wattroff(alternate_window, A_STANDOUT);
      wattron(alternate_window, COLOR_PAIR(COLOR_ERROR));
      color = COLOR_ERROR;
    }
    wattroff(alternate_window, A_STANDOUT);
    ++index;
  });
}

void change_alternate_window()
{
  debug("Changing alternate window (%d)\n", alternate_mode);
  switch (alternate_mode) {
  case VERSION:
    print_versions(filtered_packages.at(selected_package));
    break;
  case DEPENDENCIES:
    get_dependencies(filtered_packages.at(selected_package));
    break;
  case INFO:
    get_info(filtered_packages.at(selected_package));
    break;
  case VERSION_CHECK:
    selected_alternate_row = selected_package;
    print_alternate();
    break;
  }
}

void print_package_bar()
{
  const USHORT package_bar_length = (alternate_window ? COLS / 2 - 1: COLS) - 13;
  const bool filtered = search_packages.string.length() > 0;
  string package_bar_info;

  if (regex_parse_error.length() > 0) {
    package_bar_info = regex_parse_error;
  } else if (filtered_packages.size() == 0) {
    package_bar_info = "No matches";
  } else {
    const int package_number_width = number_width(pkgs.size());
    char x_of_y[32];
    snprintf(x_of_y, 32, "%d/%-*zu", selected_package + 1, package_number_width, filtered_packages.size());

    if (search_packages.string.length() > 0) {
      const size_t len = strlen(x_of_y);
      snprintf(x_of_y + len, 32, " (%zu)", pkgs.size());
    }

    package_bar_info = x_of_y;
  }

  attron(COLOR_PAIR(COLOR_INFO_BAR));
  if (alternate_mode != DEPENDENCIES) {
    mvprintw(LIST_HEIGHT, 0, "%*s", COLS, ""); // Clear and set background on the entire line
  }
  mvprintw(LIST_HEIGHT, 0, " [%s] %*s ", filtered ? "filtered" : "packages", package_bar_length, package_bar_info.c_str());
  attroff(COLOR_PAIR(COLOR_INFO_BAR));
}

void skip_empty_rows(USHORT &start_alternate, short adjustment)
{
  if (alternate_rows.at(selected_alternate_row) == "") {
    start_alternate += adjustment;
    selected_alternate_row += adjustment;
  }

  print_alternate();
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

void select_dependency_node(string &selected)
{
  if (selected == "") return;

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
  char buffer[COMMAND_SIZE];
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
  char buffer[COMMAND_SIZE];
  FILE *output = popen(command.c_str(), "r");
  setvbuf(output, buffer, _IOLBF, sizeof(buffer));

  debug("Executing sync \"%s\"\n", command.c_str());

  while (fgets(buffer, sizeof(buffer), output) != NULL) {
    size_t last = strlen(buffer) - 1;
    buffer[last] = '\0';
    callback(buffer);
  }

  return pclose(output);
}

const USHORT number_width(USHORT number)
{
  USHORT rest = number;
  USHORT i = 0;

  while (rest > 0) {
    rest /= 10;
    ++i;
  }

  return i;
}

string escape_slashes(string str)
{
  regex slash ("/");
  return regex_replace(str, slash, "\\/");
}

void package_window_up()
{
  if (filtered_packages.size() > 0) {
    selected_package = (selected_package - 1 + filtered_packages.size()) % filtered_packages.size();
    refresh_packages = true;
  }
}

void package_window_down()
{
  if (filtered_packages.size() > 0) {
    selected_package = (selected_package + 1) % filtered_packages.size();
    refresh_packages = true;
  }
}

void alternate_window_up()
{
  selected_alternate_row = max(selected_alternate_row - 1, 0);
  if (alternate_rows.at(selected_alternate_row) == "") {
    selected_alternate_row = max(selected_alternate_row - 1, 0);
  }

  print_alternate();
}

void alternate_window_down()
{
  selected_alternate_row = min(selected_alternate_row + 1, (int) alternate_rows.size() - 1);
  if (alternate_rows.at(selected_alternate_row) == "") {
    selected_alternate_row = min(selected_alternate_row + 1, (int) alternate_rows.size() - 1);
  }

  print_alternate();
}

void render_alternate_window_border()
{
  attron(COLOR_PAIR(COLOR_INFO_BAR));
  for (int i = 0; i < LIST_HEIGHT; ++i) {
    mvprintw(i, COLS / 2 - 1, "│");
  }
  attroff(COLOR_PAIR(COLOR_INFO_BAR));
}

const char* alternate_mode_to_string()
{
  switch (alternate_mode) {
    case VERSION:
      return "versions";
    case DEPENDENCIES:
      return "dependencies";
    case INFO:
      return "info";
    case VERSION_CHECK:
      return "version check";
  }
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

void getch_blocking_mode(bool should_block)
{
  nodelay(stdscr, !should_block);
}

int exit()
{
  terminate_logging();
  if (alternate_window) {
    delwin(alternate_window);
  }
  return endwin();
}
