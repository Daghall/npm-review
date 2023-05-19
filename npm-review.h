#include <string>

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
};

#define STR(x) #x

// Functions
void initialize();
void read_packages(MAX_LENGTH *foo);
vector<string> split_string(string package_string);
vector<string> shell_command(const string);
void print_alternate(PACKAGE package);
void get_versions(PACKAGE package);
void get_dependencies(PACKAGE package, bool hide_sub_dependencies = false);
void get_info(PACKAGE package);
void change_alternate_window();
int sync_shell_command(const string command, std::function<void(char*)> callback);
void install_package(PACKAGE package, const string new_version);
void uninstall_package(PACKAGE package);
bool confirm(string message);
void show_message(string message, const USHORT color = COLOR_DEFAULT);
void show_searchsting(string search_string);
void clear_message();
const unsigned short number_width(unsigned short number_of_packages);
string escape_slashes(string str);
bool is_printable(char character);
void show_cursor();
void hide_cursor();
int exit();
