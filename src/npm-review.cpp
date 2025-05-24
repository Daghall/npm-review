#include <algorithm>
#include <cstdlib>
#include <regex>
#include <signal.h>
#include <string>

#include "debug.h"
#include "ncurses.h"
#include "npm.h"
#include "npm-review.h"
#include "search.h"
#include "string.h"
#include "types.h"

bool fake_http_requests = false;

#define ctrl(c)       (c & 0x1f)

VIEW view = {
  .alternate_window = NULL,
  .package_window = NULL,
  .selected_package = 0,
  .selected_alternate_row = 0,
  .start_alternate = 0,
  .start_packages = 0,
  .main_mode = PACKAGES,
  .refresh_packages = true,
  .cursor_position = 1,
  .config_file = "package.json",
};

regex dependency_root_pattern ("^(└|├)");

int main(int argc, const char *argv[]) {
  string initial_search_string;

  while (--argc > 0) {
    switch (argv[argc][0]) {
      case '-':
        switch (argv[argc][1]) {
          case 'c':
            // TODO: Handle missing argument
            view.config_file = argv[argc + 1];
            break;
          case 'd':
            init_logging();
            break;
          case 'f':
            fake_http_requests = true;
            break;
          case 'p':
          {
            view.pkgs.push_back({
              .name = argv[argc + 1],
            });
            break;
          }
          case 'V':
            view.list_versions = true;
            break;
        }
        break;
      case '/':
        initial_search_string = argv[argc];
        break;
    }
  }
  view.searching = new Search(&view.alternate_window);
  if (initial_search_string != "") {
    view.searching->package.string = initial_search_string.substr(1);
  }

  initialize();

  MAX_LENGTH max_length;
  max_length.name = 0;
  max_length.version = 0;

  if (view.pkgs.size() == 1) {
    show_message("Fetching versions for " + view.pkgs.at(0).name + "...");
    view.alternate_mode = VERSION;
    render_package_bar();
    render_alternate_bar();
    // TODO: Handle invalid package name
    print_versions(view.pkgs.at(0), 0);
  } else {
    read_packages(&max_length, view);
  }

  // TODO: Sorting?
  // TODO: Cache "views"
  // TODO: Help screen
  // TODO: Timeout on network requests?
  // TODO: Clear cache and reload from network/disk on ctrl-l
  // TODO: gx – open URL?

  const USHORT package_size = (short) view.pkgs.size();
  const USHORT number_of_packages = max(LIST_HEIGHT, package_size);
  view.package_window = newpad(number_of_packages, 1024);
  debug("Number of packages: %d\n", number_of_packages);

  string key_sequence = "";

  while (true) {
    // Filtering packages
    try {
      if (view.main_mode == INSTALL) {
        debug("Install mode\n");
        search_for_package(max_length, view, fake_http_requests);
      } else {
      view.filtered_packages.clear();
        regex search_regex (view.searching->package.string);
        view.regex_parse_error = "";
        copy_if(view.pkgs.begin(), view.pkgs.end(), back_inserter(view.filtered_packages), [&search_regex](PACKAGE &package) {
          return regex_search(package.name + package.alias, search_regex);
        });
      }
    } catch (const regex_error &e) {
      view.regex_parse_error = e.what();
    }

    // Clamp selected package
    const USHORT number_of_filtered_packages = (USHORT) view.filtered_packages.size();

    if (view.selected_package > number_of_filtered_packages - 1) {
      view.selected_package = number_of_filtered_packages - 1;

      if (number_of_filtered_packages < LIST_HEIGHT) {
        view.start_packages = 0;
      }
    }

    // Show key sequence?
    if (key_sequence != "") {
      show_key_sequence(key_sequence);
    }

    // Render packages
    wclear(view.package_window);
    USHORT package_index = 0;

    for_each(view.filtered_packages.begin(), view.filtered_packages.end(), [&package_index, &max_length](PACKAGE &package) {
      wattron(view.package_window, COLOR_PAIR(COLOR_PACKAGE));
      wattroff(view.package_window, A_STANDOUT | A_BOLD);

      if (package_index++ == view.selected_package) {
        wattron(view.package_window, A_STANDOUT);
      }

      if (package.original_version != "") {
        wattron(view.package_window, COLOR_PAIR(COLOR_EDITED_PACKAGE) | A_BOLD);
      }
      const string package_name = package.name != package.alias
        ? package.name + " (as " + package.alias + ")"
        : package.name;
      const USHORT name_max_length = view.main_mode == INSTALL ? max_length.search : max_length.name;
      wprintw(view.package_window, " %-*s  %-*s%-*s \n", name_max_length, package_name.c_str(), max_length.version, package.version.c_str(), COLS, package.is_dev ? "  (DEV)" : "");
    });

    // Package scrolling
    if (view.selected_package <= 0) {
      view.start_packages = 0;
    } else if (view.selected_package >= LIST_HEIGHT + view.start_packages) {
      view.start_packages = view.selected_package - LIST_HEIGHT + 1;
    } else if (view.start_packages > view.selected_package) {
      view.start_packages = view.selected_package;
    } else if (view.selected_package < view.start_packages) {
      --view.start_packages;
    }

    // Alternate scrolling
    if (view.selected_alternate_row <= 0) {
      view.start_alternate = 0;
    } else if (view.selected_alternate_row >= LIST_HEIGHT + view.start_alternate) {
      view.start_alternate = view.selected_alternate_row - LIST_HEIGHT + 1;
    } else if (view.start_alternate > view.selected_alternate_row) {
      view.start_alternate = view.selected_alternate_row;
    } else if (view.selected_alternate_row < view.start_alternate) {
      --view.start_alternate;
    }

    // Render bottom bars
    render_package_bar();

    if (view.alternate_window) {
      render_alternate_bar();
    }

    // Refresh package window
    if (view.refresh_packages) {
      debug("Refreshing main... %d - %d | %d\n", view.selected_package, LIST_HEIGHT, view.start_packages);

      // Clear the window to remove residual lines when refreshing pad smaller
      // than the full height
      for (int i = view.filtered_packages.size() - view.start_packages; i < LIST_HEIGHT; ++i) {
        move(i, 0);
        clrtoeol();
      }
      refresh();

      if (view.alternate_window) {
        // Only refresh the part that is visible
        prefresh(view.package_window, view.start_packages, 0, 0, 0, LIST_HEIGHT - 1, COLS / 2 - 2);
        render_alternate_window_border();
      } else {
        prefresh(view.package_window, view.start_packages, 0, 0, 0, LIST_HEIGHT - 1, COLS - 1);
      }

      view.refresh_packages = false;
    }

    // Refresh alternate window
    if (view.alternate_window) {
      debug("Refreshing alternate... %d - %d | %d\n", view.selected_alternate_row, LIST_HEIGHT, view.start_alternate);

      // Clear the window to remove residual lines when refreshing pad smaller
      // than the full height
      for (int i = view.alternate_rows.size() - view.start_alternate; i < LIST_HEIGHT; ++i) {
        move(i, COLS / 2);
        clrtoeol();
      }

      refresh();
      prefresh(view.alternate_window, view.start_alternate, 0, 0, COLS / 2, LIST_HEIGHT - 1 , COLS - 1);
    }

    // Searching
    if (*view.searching->get_active_string() != "" && !is_message_shown()) {
      view.searching->show_search_string();
    }

    if (view.searching->is_search_mode()) {
      move(LAST_LINE, view.cursor_position);
    }

    // List versions mode
    if (view.list_versions) {
      get_all_versions();

      if (view.list_versions && getch() == ctrl('c')) {
        view.list_versions = false;
        show_error_message("Aborted version check");
        getch_blocking_mode(true);
        print_alternate(&view.pkgs.at(view.selected_package));
        continue;
      }
    }

    const short character = getch();

    if (character == ctrl('_')) {
      dump_screen(view);
      continue;
    }

    // Handle rezising
    if (character == KEY_RESIZE) {
      clear();
      // TODO: Warn about too small screen? 44 seems to be minimum width
      debug("Resizing... Columns: %d, Lines: %d\n", COLS, LINES);
      view.refresh_packages = true;
      continue;
    } else if (character == ERR) {
      getch_blocking_mode(true);
      continue;
    }

    // Handle key sequences
    if (key_sequence.length() > 0) {
      key_sequence += character;
      debug("Key sequence: %s (%d)\n", key_sequence.c_str(), H(key_sequence.c_str()));

      if (view.alternate_window) {
        switch (H(key_sequence.c_str())) {
          case H("gg"):
            view.selected_alternate_row = 0;
            print_alternate();

            if (view.alternate_mode == VERSION_CHECK) {
              view.selected_package = view.selected_alternate_row;
              view.refresh_packages = true;
            }
            break;
          case H("gc"):
            {
              if (view.alternate_mode != VERSION) {
                debug("Key sequence ignored, inapplicable mode\n");
                break;
              }

              vector<string>::iterator current = find(view.alternate_rows.begin(), view.alternate_rows.end(), view.filtered_packages.at(view.selected_package).version);
              view.selected_alternate_row = distance(current, view.alternate_rows.begin());
              print_alternate();
              break;
            }
          case H("gj"):
            {
              switch (view.alternate_mode) {
                case VERSION:
                {
                  const int major_version = atoi(view.alternate_rows.at(view.selected_alternate_row).c_str());
                  vector<string>::iterator next_major = find_if(view.alternate_rows.begin() + view.selected_alternate_row, view.alternate_rows.end(), [&major_version](string item) {
                    return atoi(item.c_str()) != major_version;
                  });

                  if (next_major == view.alternate_rows.end()) {
                    view.selected_alternate_row = view.alternate_rows.size() - 1;
                    show_error_message("Hit bottom, no more versions");
                  } else {
                    view.selected_alternate_row = distance(view.alternate_rows.begin(), next_major);
                  }

                  print_alternate();
                  break;
                }
                case DEPENDENCIES:
                {
                  vector<string>::iterator next_root_branch = find_if(view.alternate_rows.begin() + view.selected_alternate_row + 1, view.alternate_rows.end(), [](string item) {
                    return regex_search(item, dependency_root_pattern);
                  });

                  if (next_root_branch == view.alternate_rows.end()) {
                    show_error_message("Hit bottom, no more root branches");
                  } else {
                    view.selected_alternate_row = distance(view.alternate_rows.begin(), next_root_branch);
                  }

                  print_alternate();
                  break;
                }
                case INFO:
                {
                  vector<string>::iterator next_paragraph = find_if(view.alternate_rows.begin() + view.selected_alternate_row + 1, view.alternate_rows.end(), [](string item) {
                    return item == "";
                  });

                  if (next_paragraph == view.alternate_rows.end()) {
                    show_error_message("Hit bottom, no more paragraphs");
                  } else {
                    short next = distance(view.alternate_rows.begin(), next_paragraph) + 1;
                    if (next < view.alternate_rows.size()) {
                      view.selected_alternate_row = next;
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
              switch (view.alternate_mode) {
                case VERSION:
                {
                  const int major_version = atoi(view.alternate_rows.at(view.selected_alternate_row).c_str());
                  vector<string>::reverse_iterator previous_major = find_if(view.alternate_rows.rbegin() + view.alternate_rows.size() - view.selected_alternate_row, view.alternate_rows.rend(), [&major_version](string item) {
                    return atoi(item.c_str()) != major_version;
                  });

                  if (previous_major == view.alternate_rows.rend()) {
                    view.selected_alternate_row = 0;
                    show_error_message("Hit top, no more versions");
                  } else {
                    view.selected_alternate_row = distance(previous_major, view.alternate_rows.rend() - 1);
                  }

                  print_alternate();
                }
                case DEPENDENCIES:
                {
                  vector<string>::reverse_iterator previous_root_branch = find_if(view.alternate_rows.rbegin() + view.alternate_rows.size() - view.selected_alternate_row, view.alternate_rows.rend(), [](string item) {
                    return regex_search(item, dependency_root_pattern);
                  });

                  if (previous_root_branch == view.alternate_rows.rend()) {
                    show_error_message("Hit top, no more root branches");
                  } else {
                    view.selected_alternate_row = distance(previous_root_branch, view.alternate_rows.rend() - 1);
                  }

                  print_alternate();
                  break;
                }
                case INFO:
                {
                  vector<string>::reverse_iterator this_paragraph = find_if(view.alternate_rows.rbegin() + view.alternate_rows.size() - view.selected_alternate_row, view.alternate_rows.rend(), [](string item) {
                    return item == "";
                  });

                  if (this_paragraph == view.alternate_rows.rend()) {
                    show_error_message("Hit top, no more paragraphs");
                    break;
                  }

                  vector<string>::reverse_iterator previous_paragraph = find_if(this_paragraph + 1, view.alternate_rows.rend(), [](string item) {
                    return item == "";
                  });

                  if (previous_paragraph == view.alternate_rows.rend()) {
                    view.selected_alternate_row = 0;
                  } else {
                    short previous = distance(previous_paragraph, view.alternate_rows.rend());
                    if (previous > 0) {
                      view.selected_alternate_row = previous;
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
            }
          case H("zt"):
            view.start_alternate = view.selected_alternate_row;
            break;
          case H("zz"):
            view.start_alternate = max(0, view.selected_alternate_row - LIST_HEIGHT / 2);
            break;
          case H("zb"):
            view.start_alternate = max(0, view.selected_alternate_row - LIST_HEIGHT);
            break;
          default:
            debug("Unrecognized sequence (%d)\n", H(key_sequence.c_str()));
        }
      } else { // Package window
        switch (H(key_sequence.c_str())) {
          case H("gg"):
            view.selected_package = 0;
            view.refresh_packages = true;
            break;
          case H("gj"):
            package_window_down();
            break;
          case H("gk"):
            package_window_up();
            break;
          case H("zt"):
            view.start_packages = view.selected_package;
            view.refresh_packages = true;
            break;
          case H("zz"):
            view.start_packages = max(0, view.selected_package - LIST_HEIGHT / 2);
            view.refresh_packages = true;
            break;
          case H("zb"):
            view.start_packages = max(0, view.selected_package - LIST_HEIGHT);
            view.refresh_packages = true;
            break;
          default:
            debug("Unrecognized sequence\n");
        }
      }

      key_sequence= "";
      clear_key_sequence();
      continue;
    }

    // Handle non-sequences
    if (view.searching->is_search_mode()) {
      debug_key(character, "search");
      view.refresh_packages = true;

      switch (character) {
        case ctrl('c'):
        case KEY_ESC:
          view.searching->clear();
          clear_message();
          hide_cursor();
          if (view.alternate_window) {
            print_alternate();
          }
          abort_install(view);
          break;
        case ctrl('z'):
          view.searching->disable();
          hide_cursor();
          raise(SIGTSTP);
        case '\n':
          view.searching->finalize();
          if (*view.searching->get_active_string() == "") {
            show_error_message("Ignoring empty pattern");
            abort_install(view);
          }
          hide_cursor();
          break;
        case KEY_DELETE:
        case KEY_BACKSPACE:
        case '\b': {
          if (view.cursor_position > 1) {
            --view.cursor_position;
            view.searching->update_search_string(view.cursor_position);
            clear_message();
            view.searching->show_search_string();
            if (view.alternate_window) {
              print_alternate();
            }
          } else {
            view.searching->disable();
            view.selected_package = 0;
            clear_message();
            hide_cursor();

            abort_install(view);
          }
          break;
        }
        case KEY_LEFT:
          if (view.cursor_position > 1) {
            --view.cursor_position;
          }
          break;
        case KEY_RIGHT:
          if (view.cursor_position <= view.searching->get_active_string()->length()) {
            ++view.cursor_position;
          }
          break;
        case KEY_UP:
          view.cursor_position = view.searching->history_prev();
          break;
        case KEY_DOWN:
          view.cursor_position = view.searching->history_next();
          break;
        default:
          if (is_printable(character)) {
            view.searching->update_search_string(view.cursor_position++, character);
            view.searching->show_search_string();
            if (view.alternate_window) {
              print_alternate();
            }
          }
      }
      debug("view.Searching for \"%s\"\n", view.searching->get_active_string()->c_str());
    } else if (view.alternate_window) {
      debug_key(character, "alternate window");
      clear_message();

      bool install_dev_dependency = false;

      switch (character) {
        case ctrl('z'):
          raise(SIGTSTP);
          break;
        case KEY_DOWN:
        case 'J':
          package_window_down();
          change_alternate_window();
          if (view.alternate_mode != VERSION_CHECK) {
            view.start_alternate = 0;
          }
          break;
        case KEY_UP:
        case 'K':
          package_window_up();
          change_alternate_window();
          if (view.alternate_mode != VERSION_CHECK) {
            view.start_alternate = 0;
          }
          break;
        case 'j':
          alternate_window_down();

          if (view.alternate_mode == VERSION_CHECK) {
            view.selected_package = view.selected_alternate_row;
            view.refresh_packages = true;
          }
          break;
        case 'k':
          alternate_window_up();

          if (view.alternate_mode == VERSION_CHECK) {
            view.selected_package = view.selected_alternate_row;
            view.refresh_packages = true;
          }
          break;
        case ctrl('e'):
          if (view.alternate_mode == VERSION_CHECK) continue;

          view.start_alternate = min((size_t) view.start_alternate + 1, view.alternate_rows.size() - 1);

            if (view.start_alternate > view.selected_alternate_row ) {
              ++view.selected_alternate_row;
              skip_empty_rows(view, 1);
            }
          break;
        case ctrl('y'):
          if (view.alternate_mode == VERSION_CHECK) continue;

          if (view.start_alternate > 0) {
            --view.start_alternate;

            if (view.start_alternate + LIST_HEIGHT == view.selected_alternate_row) {
              --view.selected_alternate_row;
              skip_empty_rows(view, -1);
            }
          }
          break;
        case ctrl('d'):
          view.start_alternate = min((size_t) view.start_alternate + LIST_HEIGHT / 2, view.alternate_rows.size() - 1);
          if (view.start_alternate > view.selected_alternate_row) {
            view.selected_alternate_row = min((size_t) view.start_alternate, view.alternate_rows.size());
          }

          if (view.alternate_mode == VERSION_CHECK) {
            view.start_packages = view.start_alternate;
            view.selected_package = view.selected_alternate_row;
            view.refresh_packages = true;
          }

          print_alternate();
          break;
        case ctrl('u'):
          view.start_alternate = max(view.start_alternate - LIST_HEIGHT / 2, 0);
          if (view.start_alternate < view.selected_alternate_row - LIST_HEIGHT) {
            view.selected_alternate_row = view.start_alternate + LIST_HEIGHT - 1;
          }
          if (view.alternate_mode == VERSION_CHECK) {
            view.start_packages = view.start_alternate;
            view.selected_package = view.selected_alternate_row;
            view.refresh_packages = true;
          }
          print_alternate();
          break;
        case 'h':
          if (view.alternate_mode == DEPENDENCIES) {
            if (view.show_sub_dependencies) {
              view.show_sub_dependencies = false;
              get_dependencies(view, false, fake_http_requests);
            } else {
              close_alternate_window(&view.alternate_window);
              view.refresh_packages = true;
            }
          }
          break;
        case 'l':
          if (view.alternate_mode == DEPENDENCIES && !view.show_sub_dependencies) {
            view.show_sub_dependencies = true;
              get_dependencies(view, false, fake_http_requests);
          }
          break;
        case 'n':
          {
            if (view.searching->alternate.reverse) {
              view.searching->search_hit_prev(view.selected_alternate_row);
            } else {
              view.searching->search_hit_next(view.selected_alternate_row);
            }

            print_alternate();
            break;
          }
        case 'N':
          {
            if (view.searching->alternate.reverse) {
              view.searching->search_hit_next(view.selected_alternate_row);
            } else {
              view.searching->search_hit_prev(view.selected_alternate_row);
            }

            print_alternate();
            break;
          }
        case 'q':
          close_alternate_window(&view.alternate_window);
          view.refresh_packages = true;
          break;
        case ctrl('c'):
        case 'Q':
          return exit();
        case 'D':
          if (view.main_mode != INSTALL) {
            show_error_message("Development dependency install is only available for new packages");
            break;
          }
          install_dev_dependency = true;
        case '\n':
          switch (view.alternate_mode) {
            case VERSION:
              install_package(view, install_dev_dependency);
              view.refresh_packages = true;
              break;
            case DEPENDENCIES:
            case INFO:
              break;
            case VERSION_CHECK:
              view.alternate_mode = VERSION;
              print_versions(view.filtered_packages.at(view.selected_package));
              break;
          }
          break;
        case 'u':
          {
            PACKAGE package = view.filtered_packages.at(view.selected_package);
            view.refresh_packages = revert_package(package, view);
            break;
          }
        case 'g':
        case 'z':
          key_sequence = character;
          show_key_sequence(key_sequence);
          break;
        case 'G':
          view.selected_alternate_row = (int) view.alternate_rows.size() - 1;
          print_alternate();

          if (view.alternate_mode == VERSION_CHECK) {
            view.selected_package = view.selected_alternate_row;
            view.refresh_packages = true;
          }
          break;
        case '?':
          view.cursor_position = view.searching->initialize_search_reverse();
          break;
        case '/':
          view.cursor_position = view.searching->initialize_search();
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
        case 'i':
          if (view.filtered_packages.size() == 0) continue;
          view.alternate_mode = INFO;
          get_info(view.filtered_packages.at(view.selected_package), view, fake_http_requests);
          break;
        case 'I':
          view.cursor_position = view.searching->initialize_search();
          view.main_mode = INSTALL;
          view.filtered_packages.clear();
          view.refresh_packages = true;
          break;
        case 'l':
          if (view.filtered_packages.size() == 0) continue;
          view.alternate_mode = DEPENDENCIES;
          get_dependencies(view, true, fake_http_requests);
          break;
        case '\n':
          if (view.filtered_packages.size() == 0) continue;
          view.alternate_mode = VERSION;
          print_versions(view.filtered_packages.at(view.selected_package));
          break;
        case 'D':
        {
          if (view.main_mode == INSTALL) {
            // TODO: Install @latest instead - prompt?
            show_error_message("Only installed packages can be uninstalled");
            break;
          }
          if (view.filtered_packages.size() == 0) continue;
          uninstall_package(view);
          view.refresh_packages = true;
          break;
        }
        case ctrl('e'):
          view.start_packages = min(view.filtered_packages.size() - 1, (size_t) ++view.start_packages);

          if (view.start_packages > view.selected_package) {
            view.selected_package = view.start_packages;
          }
          view.refresh_packages = true;
          break;
        case ctrl('y'):
          view.start_packages = max(0, view.start_packages - 1);

          if (view.start_packages == view.selected_package - LIST_HEIGHT) {
            view.selected_package = view.start_packages + LIST_HEIGHT - 1;
          }

          view.refresh_packages = true;
          break;
        case ctrl('d'):
          view.start_packages = min((size_t) view.start_packages + LIST_HEIGHT / 2, view.pkgs.size() - 1);

          if (view.start_packages > view.selected_package) {
            view.selected_package = min((size_t) view.start_packages, view.pkgs.size());
          }
          view.refresh_packages = true;
          break;
        case ctrl('u'):
          view.start_packages = max(view.start_packages - LIST_HEIGHT / 2, 0);

          if (view.start_packages < view.selected_package - LIST_HEIGHT) {
            view.selected_package = view.start_packages + LIST_HEIGHT - 1;
          }
          view.refresh_packages = true;
          break;
        case 'p':
          if (view.main_mode == INSTALL) {
            view.main_mode = PACKAGES;
            view.searching->clear();
            view.refresh_packages = true;
          }
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
          view.selected_package = number_of_packages - 1;
          view.refresh_packages = true;
          break;
        case '?':
          view.cursor_position = view.searching->initialize_search_reverse();
          break;
        case '/':
          view.cursor_position = view.searching->initialize_search();
          break;
        case 'V':
          view.selected_package = 0;
          view.selected_alternate_row = 0;
          view.refresh_packages = true;
          view.list_versions = true;
          break;
        case 'u':
          PACKAGE package = view.filtered_packages.at(view.selected_package);
          view.refresh_packages = revert_package(package, view);
          break;
      }
    }
  }
}

WINDOW* get_alternate_window()
{
  return view.alternate_window;
}

void print_versions(PACKAGE package, int alternate_row)
{
  view.selected_alternate_row = alternate_row;
  view.alternate_rows = get_versions(package, fake_http_requests, VERSION);
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
  // TODO: Key-binding to install the latest minor/major? (ctrl-a?)

  getch_blocking_mode(false);

  if (view.selected_package == 0) {
    init_alternate_window(false);
    view.alternate_rows.clear();
    view.alternate_mode = VERSION_CHECK;
    view.selected_alternate_row = 0;

    for (int i = view.filtered_packages.size(); i > 0; --i) {
      view.alternate_rows.push_back("Pending...");
    }
  }

  PACKAGE package = view.filtered_packages.at(view.selected_package);
  string message = "Checking versions for \"" + package.name + "\"";
  show_message(message.c_str());
  print_alternate(&package);
  prefresh(view.alternate_window, view.start_alternate, 0, 0, COLS / 2, LIST_HEIGHT - 1 , COLS - 1);
  view.refresh_packages = true;

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

  view.alternate_rows.at(view.selected_package) = formatted_string;

  if (++view.selected_package >= view.filtered_packages.size()) {
    view.list_versions = false;
    clear_message();
    print_alternate(&view.pkgs.at(view.selected_package - 1));
  } else {
    view.selected_alternate_row = view.selected_package;
  }
}

// TODO: Color unmet deps?
void print_alternate(PACKAGE *package)
{
  if (view.alternate_window) {
    wclear(view.alternate_window);
    delwin(view.alternate_window);
  }

  if (package == nullptr) {
    package = &view.filtered_packages.at(view.selected_package);
  }

  if (view.selected_alternate_row < 0 && view.main_mode == INSTALL) {
    view.selected_alternate_row = 0;
  }

  string package_version = package->version;
  const int alternate_length = max(LIST_HEIGHT, (USHORT) view.alternate_rows.size());
  size_t index = 0;
  view.alternate_window = newpad(alternate_length, 1024);

  debug("Number of alternate items/rows: %zu/%d\n", view.alternate_rows.size(), alternate_length);

  regex search_regex;
  try {
    search_regex = view.searching->alternate.string;
  } catch (const regex_error &e) {
    debug("Regex error: %s\n", e.what());
  }

  USHORT color = COLOR_DEFAULT;
  view.searching->search_hits.clear();

  for_each(view.alternate_rows.begin(), view.alternate_rows.end(), [package_version, &index, &search_regex, &color, &package](const string &row) {
    if (row == package_version && view.alternate_mode == VERSION) {
      wattron(view.alternate_window, COLOR_PAIR(COLOR_CURRENT_VERSION));
      color = COLOR_CURRENT_VERSION;
      if (view.selected_alternate_row < 0) {
        view.selected_alternate_row = index;
      }
    }

    if (view.selected_alternate_row == index) {
      wattron(view.alternate_window, A_STANDOUT);
    }

    if (view.searching->alternate.string != "") {
      smatch matches;
      string::const_iterator start = row.cbegin();
      size_t prev = 0;

      wprintw(view.alternate_window, " ");

      while (regex_search(start, row.end(), matches, search_regex, regex_constants::match_any)) {
        const string match = matches[0];
        const string rest (start, row.cend());
        size_t match_position = matches.position(0);

        if (match.length() == 0) break;

        view.searching->search_hits.push_back(index);

        wprintw(view.alternate_window, "%s", rest.substr(0, match_position).c_str());

        wattroff(view.alternate_window, A_STANDOUT); // Color fix
        wattron(view.alternate_window, COLOR_PAIR(COLOR_SEARCH));
        wprintw(view.alternate_window, "%s", match.c_str());
        wattroff(view.alternate_window, COLOR_PAIR(COLOR_SEARCH));
        if (view.selected_alternate_row == index) wattron(view.alternate_window, A_STANDOUT); // Color fix

        if (color != COLOR_DEFAULT) {
          wattron(view.alternate_window, COLOR_PAIR(color));
        }

        prev = match_position + match.length();
        start += prev;
      }

      const string rest (start - prev, row.cend());
      wprintw(view.alternate_window, "%-*s\n", 512 - prev, rest.substr(prev).c_str());
    } else {
      string str = row;

      if (view.alternate_mode == VERSION && str == package->original_version) {
        str += " [committed]";
      }
      wprintw(view.alternate_window," %-*s \n", 512, str.c_str());
    }

    if (row == package_version && view.alternate_mode == VERSION) {
      wattroff(view.alternate_window, A_STANDOUT);
      wattron(view.alternate_window, COLOR_PAIR(COLOR_ERROR));
      color = COLOR_ERROR;
    }
    wattroff(view.alternate_window, A_STANDOUT);
    ++index;
  });
}

void change_alternate_window()
{
  debug("Changing alternate window (%d)\n", view.alternate_mode);
  switch (view.alternate_mode) {
  case VERSION:
    print_versions(view.filtered_packages.at(view.selected_package));
    break;
  case DEPENDENCIES:
    get_dependencies(view, true, fake_http_requests);
    break;
  case INFO:
    get_info(view.filtered_packages.at(view.selected_package), view, fake_http_requests);
    break;
  case VERSION_CHECK:
    view.selected_alternate_row = view.selected_package;
    print_alternate();
    break;
  }
}

void render_package_bar()
{
  const USHORT package_bar_length = (view.alternate_window ? COLS / 2 - 1: COLS) - 13;
  const string search_string = view.searching->package.string;
  bool filtered = false;
  string package_bar_info;

  if (view.main_mode != INSTALL) {
    view.main_mode = filtered ? FILTERED : PACKAGES;
    filtered = search_string.length() > 0;
  }

  if (view.regex_parse_error.length() > 0) {
    package_bar_info = view.regex_parse_error;
  } else if (view.filtered_packages.size() == 0) {
    package_bar_info = "No matches";
  } else {
    const int package_number_width = number_width(view.pkgs.size());
    char x_of_y[32];
    snprintf(x_of_y, 32, "%d/%-*zu", view.selected_package + 1, package_number_width, view.filtered_packages.size());

    if (filtered) {
      const size_t len = strlen(x_of_y);
      snprintf(x_of_y + len, 32, " (%zu)", view.pkgs.size());
    }

    package_bar_info = x_of_y;
  }

  attron(COLOR_PAIR(COLOR_INFO_BAR));
  if (view.alternate_mode != DEPENDENCIES) {
    mvprintw(LIST_HEIGHT, 0, "%*s", COLS, ""); // Clear and set background on the entire line
  }
  mvprintw(LIST_HEIGHT, 0, " [%s] %*s ", main_mode_to_string(view.main_mode), package_bar_length, package_bar_info.c_str());
  attroff(COLOR_PAIR(COLOR_INFO_BAR));
}

void render_alternate_bar()
{
  attron(COLOR_PAIR(COLOR_INFO_BAR));
  char x_of_y[32];
  snprintf(x_of_y, 32, "%d/%zu", view.selected_alternate_row + 1, view.alternate_rows.size());
  const char* alternate_mode_string = alternate_mode_to_string(view.alternate_mode);
  const USHORT offset = 5 - COLS % 2;
  mvprintw(LIST_HEIGHT, COLS / 2 - 1, "│ [%s] %*s ", alternate_mode_string, COLS / 2 - (offset + strlen(alternate_mode_string)), x_of_y);
  attroff(COLOR_PAIR(COLOR_INFO_BAR));
}

void skip_empty_rows(VIEW &view, short adjustment)
{
  if (view.alternate_rows.at(view.selected_alternate_row) == "") {
    view.start_alternate += adjustment;
    view.selected_alternate_row += adjustment;
  }

  print_alternate();
}

string find_dependency_root()
{
  if (view.alternate_rows.size() == 0) return "";

  while (view.selected_alternate_row > 0 && !regex_search(view.alternate_rows.at(view.selected_alternate_row), dependency_root_pattern)) {
    --view.selected_alternate_row;
  }

  string match = view.alternate_rows.at(view.selected_alternate_row);
  int end = match.find("  ");

  return match.substr(0, end);
}

void select_dependency_node(string &selected)
{
  if (selected == "") return;

  int size = view.alternate_rows.size();
  int selected_length = selected.length();
  string row;

  while (true) {
    row = view.alternate_rows.at(view.selected_alternate_row);
    if (row.compare(0, selected_length, selected) == 0) break;
    if (view.selected_alternate_row >= size) break;

    ++view.selected_alternate_row;
  }
}

void package_window_up()
{
  if (view.filtered_packages.size() > 0) {
    view.selected_package = (view.selected_package - 1 + view.filtered_packages.size()) % view.filtered_packages.size();
    view.refresh_packages = true;
  }
}

void package_window_down()
{
  if (view.filtered_packages.size() > 0) {
    view.selected_package = (view.selected_package + 1) % view.filtered_packages.size();
    view.refresh_packages = true;
  }
}

void alternate_window_up()
{
  view.selected_alternate_row = max(view.selected_alternate_row - 1, 0);
  if (view.alternate_rows.at(view.selected_alternate_row) == "") {
    view.selected_alternate_row = max(view.selected_alternate_row - 1, 0);
  }

  print_alternate();
}

void alternate_window_down()
{
  view.selected_alternate_row = min(view.selected_alternate_row + 1, (int) view.alternate_rows.size() - 1);
  if (view.alternate_rows.at(view.selected_alternate_row) == "") {
    view.selected_alternate_row = min(view.selected_alternate_row + 1, (int) view.alternate_rows.size() - 1);
  }

  print_alternate();
}

int exit()
{
  terminate_logging();
  if (view.alternate_window) {
    delwin(view.alternate_window);
  }
  return endwin();
}
