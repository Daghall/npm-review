#include <regex>
#include <signal.h>

#include "debug.h"
#include "ncurses.h"
#include "npm.h"
#include "npm-review.h"
#include "search.h"
#include "shell.h"
#include "string.h"

using namespace std;

bool fake_http_requests = false;

// "Constants"
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
string regex_parse_error;

Search searching(&alternate_window);
USHORT cursor_position = 1;

regex dependency_root_pattern ("^(└|├)");


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
        string search_string = argv[argc];
        searching.package.string = search_string.substr(1);
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
    render_package_bar();
    render_alternate_bar();
    // TODO: Handle invalid package name
    print_versions(pkgs.at(0), 0);
  } else {
    read_packages(&max_length, &pkgs);
  }

  // TODO: Sorting?
  // TODO: Cache "views"
  // TODO: Help screen
  // TODO: Timeout on network requests?
  // TODO: "Undo" – install original version
  // TODO: Clear cache and reload from network/disk on ctrl-l
  // TODO: gx – open URL?

  const USHORT package_size = (short) pkgs.size();
  const USHORT number_of_packages = max(LIST_HEIGHT, package_size);
  package_window = newpad(number_of_packages, 1024);
  debug("Number of packages: %d\n", number_of_packages);

  string key_sequence = "";

  while (true) {
    USHORT package_index = 0;

    // Filtering packages
    try {
      regex search_regex (searching.package.string);
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

    const string *search_string = searching.get_active_string();
    if (*search_string != "" && !is_message_shown()) {
      searching.show_search_string();
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
    render_package_bar();

    if (alternate_window) {
      render_alternate_bar();
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

    if (searching.is_search_mode()) {
      move(LAST_LINE, cursor_position);
    }

    if (list_versions) {
      get_all_versions();

      if (list_versions && getch() == ctrl('c')) {
        list_versions = false;
        show_error_message("Aborted version check");
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
              switch (alternate_mode) {
                case VERSION:
                {
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
                case DEPENDENCIES:
                {
                  vector<string>::iterator next_root_branch = find_if(alternate_rows.begin() + selected_alternate_row + 1, alternate_rows.end(), [](string item) {
                    return regex_search(item, dependency_root_pattern);
                  });

                  if (next_root_branch == alternate_rows.end()) {
                    show_error_message("Hit bottom, no more root branches");
                  } else {
                    selected_alternate_row = distance(alternate_rows.begin(), next_root_branch);
                  }

                  print_alternate();
                  break;
                }
                case INFO:
                {
                  vector<string>::iterator next_paragraph = find_if(alternate_rows.begin() + selected_alternate_row + 1, alternate_rows.end(), [](string item) {
                    return item == "";
                  });

                  if (next_paragraph == alternate_rows.end()) {
                    show_error_message("Hit bottom, no more paragraphs");
                  } else {
                    short next = distance(alternate_rows.begin(), next_paragraph) + 1;
                    if (next < alternate_rows.size()) {
                      selected_alternate_row = next;
                    }
                  }

                  print_alternate();
                  break;
                }
                default:
                  debug("Key sequence interpreted as 'j'\n");
                  alternate_window_down();
              }
              break;
            }
          case H("gk"):
            {
              switch (alternate_mode) {
                case VERSION:
                {
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
                }
                case DEPENDENCIES:
                {
                  vector<string>::reverse_iterator previous_root_branch = find_if(alternate_rows.rbegin() + alternate_rows.size() - selected_alternate_row, alternate_rows.rend(), [](string item) {
                    return regex_search(item, dependency_root_pattern);
                  });

                  if (previous_root_branch == alternate_rows.rend()) {
                    show_error_message("Hit top, no more root branches");
                  } else {
                    selected_alternate_row = distance(previous_root_branch, alternate_rows.rend() - 1);
                  }

                  print_alternate();
                  break;
                }
                case INFO:
                {
                  vector<string>::reverse_iterator this_paragraph = find_if(alternate_rows.rbegin() + alternate_rows.size() - selected_alternate_row, alternate_rows.rend(), [](string item) {
                    return item == "";
                  });

                  if (this_paragraph == alternate_rows.rend()) {
                    show_error_message("Hit top, no more paragraphs");
                    break;
                  }

                  vector<string>::reverse_iterator previous_paragraph = find_if(this_paragraph + 1, alternate_rows.rend(), [](string item) {
                    return item == "";
                  });

                  if (previous_paragraph == alternate_rows.rend()) {
                    selected_alternate_row = 0;
                  } else {
                    short previous = distance(previous_paragraph, alternate_rows.rend());
                    if (previous > 0) {
                      selected_alternate_row = previous;
                    }
                  }

                  print_alternate();
                  break;
                }
                default:
                debug("Key sequence interpreted as 'k'\n");
                alternate_window_up();
              }
              break;
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

    if (searching.is_search_mode()) {
      debug_key(character, "search");
      refresh_packages = true;
      string *search_string = searching.get_active_string();

      switch (character) {
        case ctrl('c'):
        case KEY_ESC:
          searching.clear();
          clear_message();
          hide_cursor();
          if (alternate_window) {
            print_alternate();
          }
          break;
        case ctrl('z'):
          searching.disable();
          hide_cursor();
          raise(SIGTSTP);
        case '\n':
          searching.disable();
          if (*search_string == "") {
            show_error_message("Ignoring empty pattern");
          }
          hide_cursor();
          break;
        case KEY_DELETE:
        case KEY_BACKSPACE:
        case '\b': {
          if (cursor_position > 1) {
            --cursor_position;
            search_string->erase(cursor_position - 1, 1);
            clear_message();
            searching.show_search_string();
            if (alternate_window) {
              print_alternate();
            }
          } else {
            searching.disable();
            clear_message();
            hide_cursor();
          }
          break;
        }
        case KEY_LEFT:
          if (cursor_position > 1) {
            --cursor_position;
          }
          break;
        case KEY_RIGHT:
          if (cursor_position <= searching.get_active_string()->length()) {
            ++cursor_position;
          }
          break;
        default:
          if (is_printable(character)) {
            search_string->insert(cursor_position - 1, 1, character);
            ++cursor_position;
            searching.show_search_string();
            if (alternate_window) {
              print_alternate();
            }
          }
      }
      debug("Searching for \"%s\"\n", searching.get_active_string()->c_str());
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
              get_dependencies(filtered_packages.at(selected_package), alternate_rows, selected_alternate_row, false, show_sub_dependencies, fake_http_requests);
            } else {
              close_alternate_window(&alternate_window);
              refresh_packages = true;
            }
          }
          break;
        case 'l':
          if (alternate_mode == DEPENDENCIES && !show_sub_dependencies) {
            show_sub_dependencies = true;
              get_dependencies(filtered_packages.at(selected_package), alternate_rows, selected_alternate_row, false, show_sub_dependencies, fake_http_requests);
          }
          break;
        case 'n':
          {
            if (searching.alternate.reverse) {
              searching.search_hit_prev(selected_alternate_row);
            } else {
              searching.search_hit_next(selected_alternate_row);
            }

            print_alternate();
            break;
          }
        case 'N':
          {
            if (searching.alternate.reverse) {
              searching.search_hit_next(selected_alternate_row);
            } else {
              searching.search_hit_prev(selected_alternate_row);
            }

            print_alternate();
            break;
          }
        case 'q':
          close_alternate_window(&alternate_window);
          refresh_packages = true;
          break;
        case ctrl('c'):
        case 'Q':
          return exit();
        case '\n':
          switch (alternate_mode) {
            case VERSION:
              install_package(filtered_packages.at(selected_package), alternate_rows.at(selected_alternate_row), selected_alternate_row, pkgs);
              refresh_packages = true;
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
          cursor_position = searching.get_active_string()->length() + 1;
          searching.initialize_search(true);
          break;
        case '/':
          cursor_position = searching.get_active_string()->length() + 1;
          searching.initialize_search();
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
          get_info(filtered_packages.at(selected_package), alternate_rows, selected_alternate_row, fake_http_requests);
          break;
        case 'I':
        case 'l':
          if (filtered_packages.size() == 0) continue;
          alternate_mode = DEPENDENCIES;
          get_dependencies(filtered_packages.at(selected_package), alternate_rows, selected_alternate_row, true, show_sub_dependencies, fake_http_requests);
          break;
        case '\n':
          if (filtered_packages.size() == 0) continue;
          alternate_mode = VERSION;
          print_versions(filtered_packages.at(selected_package));
          break;
        case 'D': {
          if (filtered_packages.size() == 0) continue;
          PACKAGE package = filtered_packages.at(selected_package);
          uninstall_package(package, pkgs);
          refresh_packages = true;
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
          cursor_position = searching.get_active_string()->length() + 1;
          searching.initialize_search(true);
          break;
        case '/':
          cursor_position = searching.get_active_string()->length() + 1;
          searching.initialize_search();
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

void print_versions(PACKAGE package, int alternate_row)
{
  selected_alternate_row = alternate_row;
  alternate_rows = get_versions(package, fake_http_requests, VERSION);
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

// This function is called once per package, in the main loop,
// to make canceling possible.
void get_all_versions()
{
  // TODO: Key-binding to install the latest minor/major?
  // ctrl-a?

  getch_blocking_mode(false); // TODO: Move to get_all_versions()

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

  vector<string> versions = get_versions(package, fake_http_requests, VERSION_CHECK);
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
    search_regex = searching.alternate.string;
  } catch (const regex_error &e) {
    debug("Regex error: %s\n", e.what());
  }

  USHORT color = COLOR_DEFAULT;
  searching.search_hits.clear();

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

    if (searching.alternate.string != "") {
      smatch matches;
      string::const_iterator start = row.cbegin();
      size_t prev = 0;

      wprintw(alternate_window, " ");

      while (regex_search(start, row.end(), matches, search_regex, regex_constants::match_any)) {
        const string match = matches[0];
        const string rest (start, row.cend());
        size_t match_position = matches.position(0);

        if (match.length() == 0) break;

        searching.search_hits.push_back(index);

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
    get_dependencies(filtered_packages.at(selected_package), alternate_rows, selected_alternate_row, true, show_sub_dependencies, fake_http_requests);
    break;
  case INFO:
    get_info(filtered_packages.at(selected_package), alternate_rows, selected_alternate_row, fake_http_requests);
    break;
  case VERSION_CHECK:
    selected_alternate_row = selected_package;
    print_alternate();
    break;
  }
}

void render_package_bar()
{
  const USHORT package_bar_length = (alternate_window ? COLS / 2 - 1: COLS) - 13;
  const string search_string = searching.package.string;
  const bool filtered = search_string.length() > 0;
  string package_bar_info;

  if (regex_parse_error.length() > 0) {
    package_bar_info = regex_parse_error;
  } else if (filtered_packages.size() == 0) {
    package_bar_info = "No matches";
  } else {
    const int package_number_width = number_width(pkgs.size());
    char x_of_y[32];
    snprintf(x_of_y, 32, "%d/%-*zu", selected_package + 1, package_number_width, filtered_packages.size());

    if (search_string.length() > 0) {
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

void render_alternate_bar()
{
  attron(COLOR_PAIR(COLOR_INFO_BAR));
  char x_of_y[32];
  snprintf(x_of_y, 32, "%d/%zu", selected_alternate_row + 1, alternate_rows.size());
  const char* alternate_mode_string = alternate_mode_to_string(alternate_mode);
  const USHORT offset = 5 - COLS % 2;
  mvprintw(LIST_HEIGHT, COLS / 2 - 1, "│ [%s] %*s ", alternate_mode_string, COLS / 2 - (offset + strlen(alternate_mode_string)), x_of_y);
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

  while (selected_alternate_row > 0 && !regex_search(alternate_rows.at(selected_alternate_row), dependency_root_pattern)) {
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

int exit()
{
  terminate_logging();
  if (alternate_window) {
    delwin(alternate_window);
  }
  return endwin();
}
