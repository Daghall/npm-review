#include <string>
#include <vector>

using namespace std;

// Consants
const unsigned short COLOR_DEFAULT = -1;
const unsigned short COLOR_SELECTED_PACKAGE = 1;
const unsigned short COLOR_PACKAGE = 2;
const unsigned short COLOR_OLD_VERSION = 3;
const unsigned short COLOR_CURRENT_VERSION = 4;
const unsigned short COLOR_INFO_BAR = 5;

const unsigned short BOTTOM_BAR_HEIGHT = 2;
const unsigned short KEY_ESC = 0x1B;
const unsigned short KEY_DELETE = 0x7F;

// Types
typedef struct {
  string name;
  string version;
  bool is_dev;
} PACKAGE;

typedef struct {
  unsigned short name;
  unsigned short version;
} MAX_LENGTH;

typedef unsigned short USHORT;

enum alternate_modes {
  VERSION,
  DEPENDENCIES,
  INFO,
  VERSION_CHECK,
};

#define STR(x) #x

// Functions
void initialize();
void read_packages(MAX_LENGTH *foo);
vector<string> split_string(string package_string);
vector<string> shell_command(const string);
void print_alternate(PACKAGE package);
vector<string> get_versions(PACKAGE package);
void print_versions(PACKAGE package);
void get_dependencies(PACKAGE package, bool init = true);
void get_info(PACKAGE package);
void get_all_versions();
string get_major(string semver);
void change_alternate_window();
void skip_empty_rows(unsigned short &start_alternate, short adjustment);
string find_dependency_root();
void select_dependency_node(string &selected);
int sync_shell_command(const string command, std::function<void(char*)> callback);
void install_package(PACKAGE package, const string new_version);
void uninstall_package(PACKAGE package);
bool confirm(string message);
void show_message(string message, const USHORT color = COLOR_DEFAULT);
void show_searchsting();
void clear_message();
void close_alternate_window();
const unsigned short number_width(unsigned short number_of_packages);
string escape_slashes(string str);
bool is_printable(char character);
void show_cursor();
void hide_cursor();
void getch_blocking_mode(bool should_block);
int exit();
