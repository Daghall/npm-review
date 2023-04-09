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
void hide_cursor();
int exit();

// Debugging
FILE *debug_log = NULL;
#define debug(...) if (debug_log != NULL) { \
  fprintf(debug_log, __VA_ARGS__); \
  fflush(debug_log); \
}

const unsigned short COLOR_DEFAULT = -1;
const unsigned short COLOR_SELECTED_PACKAGE = 1;
const unsigned short COLOR_PACKAGE = 2;
const unsigned short COLOR_OLD_VERSION = 3;
const unsigned short COLOR_CURRENT_VERSION = 4;
const unsigned short COLOR_INFO_BAR = 5;

const unsigned short BOTTOM_BAR_HEIGHT = 2;

struct pkg {
  string name;
  string version;
  bool is_dev;
};
struct ml {
  unsigned short name;
  unsigned short version;
};

WINDOW *version_window = NULL;
WINDOW *package_window = NULL;

vector<pkg> pkgs;
vector<string> versions;
unsigned short selected_package = 0;
short selected_version = 0;

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
  hide_cursor();

  init_pair(COLOR_SELECTED_PACKAGE, COLOR_BLACK, COLOR_GREEN);
  init_pair(COLOR_PACKAGE, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_OLD_VERSION, COLOR_RED, COLOR_DEFAULT);
  init_pair(COLOR_CURRENT_VERSION, COLOR_GREEN, COLOR_DEFAULT);
  init_pair(COLOR_INFO_BAR, COLOR_BLACK, COLOR_BLUE);

  const unsigned short LIST_HEIGHT = LINES - 2;

  struct ml max_length; // TODO: Simplify
  max_length.name = 0;
  max_length.version = 0;

  read_packages(&max_length);

  // TODO: Handle resize
  // TODO: Signals and/or ctrl-c/ctrl-d
  // TODO: Handle searching/filtering
  // TODO: Sorting?
  const unsigned short number_of_packages = max(LINES - BOTTOM_BAR_HEIGHT, (int) pkgs.size());
  const size_t package_number_width = 2; // TODO: Calculate properly
  package_window = newpad(number_of_packages, COLS);
  debug("Number of packages: %d\n", number_of_packages);

  unsigned short start_packages = 0;
  unsigned short start_versions = 0;

  // TODO: Fix initial rendering
  while (true) {
    unsigned short y_pos = 0;
    unsigned short index = 0;
    werase(package_window);
    for_each(pkgs.begin(), pkgs.end(), [&y_pos, &index, &max_length](pkg &package) {
      wattron(package_window, COLOR_PAIR(COLOR_PACKAGE));
      if (index == selected_package) {
        wattron(package_window, COLOR_PAIR(COLOR_SELECTED_PACKAGE));
      }
      wprintw(package_window, " %-*s  %-*s%s \n", max_length.name, package.name.c_str(), max_length.version, package.version.c_str(), package.is_dev ? "  (DEV)" : "");
      ++y_pos;
      ++index;
    });

    // Handle scrolling of packages
    if (selected_package == 0) {
      start_packages = 0;
    } else if (selected_package == number_of_packages - 1) {
      start_packages = number_of_packages - LIST_HEIGHT;
    } else if (selected_package >= LIST_HEIGHT + start_packages) {
      ++start_packages;
    } else if (selected_package < start_packages) {
      --start_packages;
    }

    // Handle scrolling of versions
    const unsigned short number_of_versions = max(LINES - BOTTOM_BAR_HEIGHT, (int) versions.size());

    if (selected_version <= 0) {
      start_versions = 0;
    } else if (selected_version == number_of_versions - 1) {
      start_versions = number_of_versions - LIST_HEIGHT;
    } else if (selected_version >= LIST_HEIGHT + start_versions) {
      start_versions = selected_version - LIST_HEIGHT + 1;
    } else if (selected_version < start_versions) {
      --start_versions;
    }

    debug("Refresh... %d - %d | %d\n", selected_package, LIST_HEIGHT, start_packages);
    prefresh(package_window, start_packages, 0, 0, 0, LIST_HEIGHT - 1, COLS);
    if (version_window) {
      debug("Refresh... %d - %d | %d\n", selected_version, LIST_HEIGHT, start_versions);
      prefresh(version_window, start_versions, 0, 0, COLS / 2 + 1, LIST_HEIGHT - 1 , COLS - 1);
    }

    attron(COLOR_PAIR(COLOR_INFO_BAR));
    // TODO: Add version X/Y
    mvprintw(LINES - 2, 0, " %*d/%-*d", package_number_width, selected_package + 1, COLS - 2 * package_number_width, number_of_packages);
    attroff(COLOR_PAIR(COLOR_INFO_BAR));

    move(LINES - 1, 0); // Move cursor to make `npm install` render here

    const unsigned int character = wgetch(stdscr);

    if (version_window) {
      debug("Sending key '%c' to version window\n", character);
      switch (character) {
        case KEY_DOWN:
        case 'J':
          // TODO: refresh package list first
          selected_package = (selected_package + 1) % pkgs.size();
          get_versions(pkgs.at(selected_package));
          start_versions = 0;
          break;
        case KEY_UP:
        case 'K':
          // TODO: refresh package list first
          selected_package = (selected_package - 1 + pkgs.size()) % pkgs.size();
          get_versions(pkgs.at(selected_package));
          start_versions = 0;
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
      debug("Sending key '%c' to main window\n", character);
      switch (character) {
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
      .is_dev = origin == ".devDependencies"
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
  mvprintw(0, COLS / 2 - 1, "  Loading...                 "); // TODO: Pretty up
  for (int i = 1; i < LINES - BOTTOM_BAR_HEIGHT; ++i) {
    mvprintw(i, COLS / 2, "                               "); // TODO: Pretty up
  }
  refresh();

  selected_version = -1;

  char command[1024];
  snprintf(command, 1024, "npm info %s versions --json | jq 'reverse | .[]' -r", package_name);

  versions = vector<string>();
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
  /* versions = shell_command(command); */
  print_versions(package);
}

void install_pagage(struct pkg package, const string new_version)
{
  char command[1024];
  snprintf(command, 1024, "npm install %s@%s --silent", package.name.c_str(), new_version.c_str());
  move(LINES - BOTTOM_BAR_HEIGHT, 0);
  sync_shell_command(command, [](char* line) {
    wprintw(version_window, "%s\n", line);
  });

  hide_cursor();

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
  int number_of_versions = max(LINES - BOTTOM_BAR_HEIGHT, (int) versions.size());
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
    /* debug(" %s \n", version.c_str()); */
    if (version == package_version) {
      wattroff(version_window, A_STANDOUT);
      wattron(version_window, COLOR_PAIR(COLOR_OLD_VERSION));
    }
    wattroff(version_window, A_STANDOUT);
    ++index;
  });
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

// Hide cursor
void hide_cursor()
{
  curs_set(1); // Explicitly show it to avoid rendering bug after `npm install`
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
