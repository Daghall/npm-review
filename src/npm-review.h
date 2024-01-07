#include <string>
#include <vector>

using namespace std;

// Types
//
typedef unsigned short USHORT;

typedef struct {
  string name;
  string version;
  bool is_dev;
} PACKAGE;

typedef struct {
  USHORT name;
  USHORT version;
} MAX_LENGTH;


enum alternate_modes {
  VERSION,
  DEPENDENCIES,
  INFO,
  VERSION_CHECK,
};

// Consants
const USHORT COLOR_DEFAULT = -1;
const USHORT COLOR_SELECTED_PACKAGE = 1;
const USHORT COLOR_PACKAGE = 2;
const USHORT COLOR_OLD_VERSION = 3;
const USHORT COLOR_CURRENT_VERSION = 4;
const USHORT COLOR_INFO_BAR = 5;

const USHORT BOTTOM_BAR_HEIGHT = 2;
const USHORT KEY_ESC = 0x1B;
const USHORT KEY_DELETE = 0x7F;

#define STR(x) #x

// Functions
void initialize();
void read_packages(MAX_LENGTH *foo);
vector<string> split_string(string package_string);
vector<string> shell_command(const string);
void print_alternate(PACKAGE package);
vector<string> get_versions(PACKAGE package);
void print_versions(PACKAGE package, int alternate_row = -1);
void get_dependencies(PACKAGE package, bool init = true);
void get_info(PACKAGE package);
void get_all_versions();
string get_major(string semver);
void change_alternate_window();
void print_package_bar(bool use_global = false);
void skip_empty_rows(USHORT &start_alternate, short adjustment);
string find_dependency_root();
void select_dependency_node(string &selected);
int sync_shell_command(const string command, std::function<void(char*)> callback);
void init_alternate_window(bool clear_screen = true);
void install_package(PACKAGE package, const string new_version);
void uninstall_package(PACKAGE package);
bool confirm(string message);
void show_message(string message, const USHORT color = COLOR_DEFAULT);
void show_searchsting();
void clear_message();
void close_alternate_window();
const USHORT number_width(USHORT number_of_packages);
string escape_slashes(string str);
bool is_printable(char character);
const char* alternate_mode_to_string();
void show_cursor();
void hide_cursor();
void getch_blocking_mode(bool should_block);
int exit();
