#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdio.h>
#include <cmath>
#include <ncurses.h>
#include <string>
#include <vector>
#include <functional>

using namespace std;

// Functions
void read_packages(struct ml *foo);
vector<string> split_string(string package_string);
vector<string> shell_command(const string);
void print_versions(struct pkg package);
void get_versions(struct pkg package);
void sync_shell_command(const string command, std::function<void(char*)> callback);
void install_pagage(struct pkg package, const string new_version);
int exit();

// Debugging
FILE *debug_log = NULL;
#define debug(...) if (debug_log != NULL) { \
  fprintf(debug_log, __VA_ARGS__); \
  fflush(debug_log); \
}

const short COLOR_DEFAULT = -1;
const short COLOR_SELECTED_PACKAGE = 1;
const short COLOR_PACKAGE = 2;
const short COLOR_OLD_VERSION = 3;
const short COLOR_CURRENT_VERSION = 4;
const short COLOR_INFO_BAR = 5;

const unsigned short BOTTOM_BAR_HEIGHT = 2;

struct pkg {
  string name;
  string version;
  bool isDev;
};
struct ml {
  int name;
  int version;
};

WINDOW *version_window = NULL;
WINDOW *package_window = NULL;

vector<pkg> pkgs;
vector<string> versions;
int selected_package = 0;
int selected_version = 0;

int main(int argc, const char *argv[])
{
  if (argc > 1 && strcmp(argv[1], "-d") == 0) {
     debug_log = fopen("./log", "w");
  }

  debug("Init\n");
  initscr();
  keypad(stdscr, true);
  start_color();
  use_default_colors();
  assume_default_colors(COLOR_WHITE, COLOR_DEFAULT);
  noecho();
  curs_set(0); // Hide cursor

  init_pair(COLOR_SELECTED_PACKAGE, COLOR_BLACK, COLOR_GREEN);
  init_pair(COLOR_PACKAGE, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_OLD_VERSION, COLOR_RED, COLOR_DEFAULT);
  init_pair(COLOR_CURRENT_VERSION, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_INFO_BAR, COLOR_BLACK, COLOR_BLUE);

  const unsigned short PACKAGE_WINDOW_HEIGHT = LINES - 2;

  struct ml max_length; // TODO: Simplify
  max_length.name = 0;
  max_length.version = 0;

  read_packages(&max_length);

  // TODO: Handle resize
  // TODO: signals and/or ctrl-c/ctrl-d
  // TODO: Handle scrolling
  int number_of_packages = max(LINES - 1, (int) pkgs.size());
  package_window = newpad(number_of_packages, COLS);
  debug("Number of packages: %d\n", number_of_packages);

  int start = 0;

  // TODO: Fix initial rendering
  while (true) {
    size_t y_pos = 0;
    size_t index = 0;
    werase(package_window);
    for_each(pkgs.begin(), pkgs.end(), [&y_pos, &index, &max_length](pkg &package) {
      wattron(package_window, COLOR_PAIR(COLOR_PACKAGE));
      if (index == selected_package) {
        wattron(package_window, COLOR_PAIR(COLOR_SELECTED_PACKAGE));
      }
      wprintw(package_window, " %-*s  %-*s%s \n", max_length.name, package.name.c_str(), max_length.version, package.version.c_str(), package.isDev ? "  (DEV)" : "");
      ++y_pos;
      ++index;
    });

    // Handle scrolling of packages
    if (selected_package == 0) {
      start = 0;
    } else if (selected_package == number_of_packages - 1) {
      start = number_of_packages - PACKAGE_WINDOW_HEIGHT;
    } else if (selected_package >= PACKAGE_WINDOW_HEIGHT + start) {
      ++start;
    } else if (selected_package < start) {
      --start;
    }

    debug("Refresh... %d - %d | %d\n", selected_package, LINES, start);
    prefresh(package_window, start, 0, 0, 0, PACKAGE_WINDOW_HEIGHT - 1, COLS - 0);
    if (version_window) {
      prefresh(version_window, 0, 0, 0, COLS / 2 + 1, LINES , COLS - 7);
    }

    const size_t package_number_width = 2; // TODO: Calculate properly
    attron(COLOR_PAIR(COLOR_INFO_BAR));
    mvprintw(LINES - 2, 0, " %*d/%-*d", package_number_width, selected_package + 1, COLS - 2 * package_number_width, number_of_packages);

    move(LINES - 1, 0); // Move cursor to make `npm install` render here

    int c = wgetch(stdscr);

    if (version_window) {
      debug("Sending key '%c' to version window\n", c);
      switch (c) {
        case KEY_DOWN:
        case 'J':
          // TODO: refresh package list first
          selected_package = (selected_package + 1) % pkgs.size();
          get_versions(pkgs.at(selected_package));
          break;
        case KEY_UP:
        case 'K':
          // TODO: refresh package list first
          selected_package = (selected_package - 1 + pkgs.size()) % pkgs.size();
          get_versions(pkgs.at(selected_package));
          break;
        case 'j':
          selected_version = min(selected_version + 1, (int) versions.size() - 1);
          print_versions(pkgs.at(selected_package));
          break;
        case 'k':
          selected_version = max(selected_version - 1, 0);
          print_versions(pkgs.at(selected_package));
          break;
        case 'q':
          wclear(version_window);
          prefresh(version_window, 0, 0, 0, COLS / 2, LINES , COLS - 7);
          delwin(version_window);
          version_window = NULL;
          break;
        case 'Q':
          return exit();
        case '\n':
          // TODO: Add confirm?
          install_pagage(pkgs.at(selected_package), versions.at(selected_version));
          break;
      }
    } else {
      debug("Sending key '%c' to main window\n", c);
      switch (c) {
        case KEY_DOWN:
        case 'j':
        case 'J':
          selected_package = (selected_package + 1) % pkgs.size();
          break;
        case KEY_UP:
        case 'k':
        case 'K':
          selected_package = (selected_package - 1 + pkgs.size()) % pkgs.size();
          break;
          break;
        case '\n':
          get_versions(pkgs.at(selected_package));
          break;
        case 'q':
        case 'Q':
          return exit();
      }
    }
  }
}

vector<string> split_string(string package_string) {
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

// Parse package name and version
void read_packages(struct ml *max_length)
{
  debug("read_packages\n");
  string command = "for dep in .dependencies .devDependencies; do jq $dep' | keys[] as $key | \"\\($key) \\(.[$key] | sub(\"[~^]\"; \"\")) '$dep'\"' -r < package.json; done";
  /* string command = "jq '.dependencies | keys_unsorted[] as $key | \"\\($key) \\(.[$key] | sub(\"[~^]\"; \"\"))\"' -r < package.json"; */
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
      .isDev = origin == ".devDependencies"
    });

    /* debug("%lu - %d\n", name.length(), max_length->name); */
    if (max_length && name.length() > max_length->name) {
      max_length->name = name.length();
    }
    if (max_length && version.length() > max_length->version) {
      max_length->version = version.length();
    }
  });
}


void get_versions(struct pkg package)
{
  const char* package_name = package.name.c_str();
  mvprintw(0, COLS / 2 - 1, "Versions:");
  mvprintw(1, COLS / 2 - 1, "  Loading...                 "); // TODO: Pretty up
  for (int i = 2; i < LINES; ++i) {
    mvprintw(i, COLS / 2, "                               "); // TODO: Pretty up
  }
  refresh();

  selected_version = -1;

  char command[1024];
  snprintf(command, 1024, "npm info %s versions --json | jq 'reverse | .[]' -r", package_name);

  versions = vector<string>();
  versions.push_back("32.0.0");
  versions.push_back("27.0.0");
  versions.push_back("22.0.0");
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
  /* versions = shell_command(command); */
  print_versions(package);
}

void install_pagage(struct pkg package, const string new_version)
{
  char command[1024];
  snprintf(command, 1024, "npm install %s@%s --silent", package.name.c_str(), new_version.c_str());
  move(0, 0);
  /* move(LINES - 2, 0); */
  wmove(version_window, LINES - 1, 0);
  version_window = newpad(LINES, COLS);
  sync_shell_command(command, [](char* line) {
    wprintw(version_window, "%s\n", line);
    prefresh(version_window, 0, 0, 1, COLS / 2, LINES - 1, COLS - 1);
  });

  read_packages(NULL);
  selected_version = -1;
  print_versions(pkgs.at(selected_package));
}

// Print versions
void print_versions(struct pkg package)
{
  string package_version = package.version;
  if (version_window) {
    wclear(version_window);
    delwin(version_window);
  }
  int number_of_lines = max(LINES - 1, (int) versions.size()); // TODO: Needed?
  debug("Number of versions: %d\n", number_of_lines);
  version_window = newpad(LINES, COLS);
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
    /* debug(" %s \n", version.c_str()); */
    if (version == package_version) {
      wattroff(version_window, A_STANDOUT);
      wattron(version_window, COLOR_PAIR(COLOR_OLD_VERSION));
    }
    wattroff(version_window, A_STANDOUT);
    ++index;
  });

  int start = 0;
  prefresh(version_window, start, 0, 1, COLS / 2, LINES - 1, COLS - 1);
}

// Shell command
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

// Sync shell command
void sync_shell_command(const string command, std::function<void(char*)> callback)
{
  char buffer[1024];
  FILE *output = popen(command.c_str(), "r");
  setvbuf(output, buffer, _IOLBF, sizeof(buffer));

  while (fgets(buffer, sizeof(buffer), output) != NULL) {
    size_t last = strlen(buffer) - 1;
    buffer[last] = '\0';
    callback(buffer);
  }

  pclose(output);
}

int exit()
{
  fclose(debug_log);
  if (version_window) {
    delwin(version_window);
  }
  return endwin();
}
