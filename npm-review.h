#include <string>

using namespace std;

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

// Functions
void initialize();
void read_packages(MAX_LENGTH *foo);
vector<string> split_string(string package_string);
vector<string> shell_command(const string);
void print_versions(PACKAGE package);
void get_versions(PACKAGE package);
int sync_shell_command(const string command, std::function<void(char*)> callback);
void install_package(PACKAGE package, const string new_version);
void uninstall_package(PACKAGE package);
const unsigned short number_width(unsigned short number_of_packages);
bool is_printable(char character);
void hide_cursor();
int exit();
