#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
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

// Debugging
FILE *debug_log = NULL;
#define debug(...)    if (debug_log != NULL) { \
  fprintf(debug_log, __VA_ARGS__); \
  fflush(debug_log); \
}
bool fake_versions = false;

// Constants
#define LIST_HEIGHT   (USHORT)(LINES - BOTTOM_BAR_HEIGHT)
#define LAST_LINE     (USHORT)(LINES - 1)
#define ctrl(c)       (c & 0x1f)

const unsigned short COLOR_DEFAULT = -1;
const unsigned short COLOR_SELECTED_PACKAGE = 1;
const unsigned short COLOR_PACKAGE = 2;
const unsigned short COLOR_OLD_VERSION = 3;
const unsigned short COLOR_CURRENT_VERSION = 4;
const unsigned short COLOR_INFO_BAR = 5;

const unsigned short BOTTOM_BAR_HEIGHT = 2;
const unsigned short KEY_ESC = 0x1B;
const unsigned short KEY_DELETE = 0x7F;

// Global variables
WINDOW *version_window = NULL;
WINDOW *package_window = NULL;
vector<PACKAGE> pkgs;
vector<PACKAGE> filtered_packages;
vector<string> versions;
unsigned short selected_package = 0;
short selected_version = 0;

int main(int argc, const char *argv[])
{
  while (--argc > 0) {
    switch (argv[argc][1]) {
      case 'd':
        debug_log = fopen("./log", "w");
        break;
      case 'f':
        fake_versions = true;
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
  // TODO: Cache versions?
  // TODO: `npm info %s` on key press of `i`
  // TODO: Show dependencies on key press of `I`?
  const unsigned short package_size = (short) pkgs.size();
  const unsigned short number_of_packages = max(LIST_HEIGHT, package_size);
  package_window = newpad(number_of_packages, COLS);
  debug("Number of packages: %d\n", number_of_packages);

  unsigned short start_packages = 0;
  unsigned short start_versions = 0;
  bool search_mode = false;
  string search_string = "";
  string message_string = "";

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

    if (search_mode) {
      mvprintw(LAST_LINE, 0, "/%s", search_string.c_str());
    } else {
      move(LAST_LINE, 0);
      clrtoeol();
      mvprintw(LAST_LINE, 0, "%s", message_string.c_str());
    }

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

    // Version scrolling
    const unsigned short number_of_versions = max(LIST_HEIGHT, (USHORT) versions.size());

    if (selected_version <= 0) {
      start_versions = 0;
    } else if (selected_version == number_of_versions - 1) {
      start_versions = number_of_versions - LIST_HEIGHT;
    } else if (selected_version >= LIST_HEIGHT + start_versions) {
      start_versions = selected_version - LIST_HEIGHT + 1;
    } else if (selected_version < start_versions) {
      --start_versions;
    }

    // Refresh windows
    debug("Refresh p... %d - %d | %d\n", selected_package, LIST_HEIGHT, start_packages);
    prefresh(package_window, start_packages, 0, 0, 0, LIST_HEIGHT - 1, COLS);
    if (version_window) {
      debug("Refresh v... %d - %d | %d\n", selected_version, LIST_HEIGHT, start_versions);
      prefresh(version_window, start_versions, 0, 0, COLS / 2 + 1, LIST_HEIGHT - 1 , COLS - 1);
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

    if (version_window) {
      const unsigned short version_number_width = number_width(versions.size());
      mvprintw(LIST_HEIGHT, COLS / 2, "  %*d/%d", version_number_width, selected_version + 1, versions.size());
    }
    attroff(COLOR_PAIR(COLOR_INFO_BAR));

    // Move to last line to make `npm install` render here
    move(LAST_LINE, 0);

    const short character = getch();

    message_string = "";

    if (search_mode) {
      debug("Sending key '%c' (%#x) to search\n", character, character);
      switch (character) {
        case ctrl('c'):
        case KEY_ESC:
          search_mode = false;
          search_string = "";
          break;
        case ctrl('z'):
          search_mode = false;
          raise(SIGTSTP);
        case '\n':
          search_mode = false;
          break;
        case KEY_DELETE:
        case KEY_BACKSPACE:
        case '\b': {
          const short last_character = search_string.length() - 1;

          if (last_character >= 0) {
            search_string.erase(last_character, 1);
            move(LAST_LINE, last_character);
            clrtoeol();
          } else {
            search_mode = false;
          }
          break;
        }
        default:
          if (is_printable(character)) {
            search_string += character;
          }
      }
      mvprintw(LAST_LINE, 0, search_string.c_str());
      debug("Searching for \"%s\"\n", search_string.c_str());
    } else if (version_window) {
      debug("Sending key '%d' (%#x) to version window\n", character, character);
      switch (character) {
        case ctrl('z'):
          raise(SIGTSTP);
          break;
        // TODO: Bail if no packages, or exit?
        case KEY_DOWN:
        case 'J':
          selected_package = (selected_package + 1) % filtered_packages.size();
          get_versions(filtered_packages.at(selected_package));
          start_versions = 0;
          break;
        case KEY_UP:
        case 'K':
          selected_package = (selected_package - 1 + filtered_packages.size()) % filtered_packages.size();
          get_versions(filtered_packages.at(selected_package));
          start_versions = 0;
          break;
        case 'j':
          selected_version = min(selected_version + 1, (int) versions.size() - 1);
          print_versions(filtered_packages.at(selected_package));
          break;
        case 'k':
          selected_version = max(selected_version - 1, 0);
          print_versions(filtered_packages.at(selected_package));
          break;
        case 'q':
          wclear(version_window);
          delwin(version_window);
          version_window = NULL;
          break;
        case ctrl('c'):
        case 'Q':
          return exit();
        case '\n':
          // TODO: Add confirm?
          install_package(filtered_packages.at(selected_package), versions.at(selected_version));
          break;
        case 'g':
          selected_version = 0;
          print_versions(filtered_packages.at(selected_package));
          break;
        case 'G':
          selected_version = number_of_versions - 1;
          print_versions(filtered_packages.at(selected_package));
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
        case '\n':
          // TODO: Bounds check?
          get_versions(filtered_packages.at(selected_package));
          break;
        case 'D': {
          // TODO: Add confirm
          PACKAGE package = filtered_packages.at(selected_package);
          message_string = "Uninstalled " + package.name;
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
          break;
      }
    }
  }
}

void initialize() {
  debug("Init\n");
  initscr();
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
  mvprintw(0, COLS / 2 - 1, "  Loading...                 "); // TODO: Pretty up
  for (int i = 1; i < LIST_HEIGHT; ++i) {
    mvprintw(i, COLS / 2, "                               "); // TODO: Pretty up
  }
  refresh();

  selected_version = -1;

  char command[1024];
  snprintf(command, 1024, "npm info %s versions --json | jq 'if (type == \"array\") then reverse | .[] else . end' -r", package_name);

  versions = vector<string>();

  if (fake_versions) {
    versions.push_back("25.0.0");
    versions.push_back("24.0.0");
    versions.push_back("23.0.0");
    versions.push_back("22.0.0");
    versions.push_back("21.0.0");
    versions.push_back("20.0.0");
    versions.push_back("19.0.0");
    versions.push_back("18.0.0");
    versions.push_back("17.0.0");
    versions.push_back("16.0.0");
    versions.push_back("15.0.0");
    versions.push_back("14.0.0");
    versions.push_back("13.0.0");
    versions.push_back("12.0.0");
    versions.push_back("12.0.0");
    versions.push_back("11.0.0");
    versions.push_back("10.0.0");
    versions.push_back("9.0.0");
    versions.push_back("8.0.0");
    versions.push_back("7.0.0");
    versions.push_back("6.0.0");
    versions.push_back("5.0.0");
    versions.push_back("4.0.0");
    versions.push_back("3.0.0");
    versions.push_back("2.0.0");
    versions.push_back("1.0.0");
  } else {
    versions = shell_command(command);
  }
  print_versions(package);
}

void install_package(PACKAGE package, const string new_version)
{
  char command[1024];
  snprintf(command, 1024, "npm install %s@%s --silent", package.name.c_str(), new_version.c_str());
  int exit_code = sync_shell_command(command, [](char* line) {
    debug("NPM INSTALL: %s\n", line);
  });

  hide_cursor();

  if (exit_code == OK) {
    read_packages(NULL);
    selected_version = -1;
    PACKAGE p = filtered_packages.at(selected_package);
    p.version = new_version;
    print_versions(p);
  } else {
    // TODO: Handle error
  }
}

void uninstall_package(PACKAGE package)
{
  char command[1024];
  snprintf(command, 1024, "npm uninstall %s --silent", package.name.c_str());
  int exit_code = sync_shell_command(command, [](char* line) {
    debug("NPM UNINSTALL: %s\n", line);
  });

  hide_cursor();

  if (exit_code == OK) {
    read_packages(NULL);
  } else {
    // TODO: Handle error
  }
}

void print_versions(PACKAGE package)
{
  string package_version = package.version;
  if (version_window) {
    wclear(version_window);
    delwin(version_window);
  }
  int number_of_versions = max(LIST_HEIGHT, (USHORT) versions.size());
  debug("Number of versions: %d\n", number_of_versions);
  version_window = newpad(number_of_versions, COLS);
  size_t index = 0;
  for_each(versions.begin(), versions.end(), [package_version, &index](string &version) {
    if (version == package_version) {
      wattron(version_window, COLOR_PAIR(COLOR_CURRENT_VERSION));
      if (selected_version < 0) {
        selected_version = index;
      }
    }
    if (selected_version == index) {
      wattron(version_window, A_STANDOUT);
    }
    wprintw(version_window," %s \n", version.c_str());
    if (version == package_version) {
      wattroff(version_window, A_STANDOUT);
      wattron(version_window, COLOR_PAIR(COLOR_OLD_VERSION));
    }
    wattroff(version_window, A_STANDOUT);
    ++index;
  });
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

bool is_printable(char character)
{
  return character >= 0x20 && character < 0x7F;
}

void hide_cursor()
{
  curs_set(1); // Explicitly show it to avoid rendering bug after `npm {un,}install`
  curs_set(0);
}

int exit()
{
  fclose(debug_log);
  if (version_window) {
    delwin(version_window);
  }
  return endwin();
}
