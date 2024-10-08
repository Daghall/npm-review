#ifndef _TYPES_H
#define _TYPES_H

#include <map>
#include <string>
#include <vector>

using namespace std;

typedef unsigned short USHORT;

#define CACHE_TYPE string, vector<string>
typedef pair<CACHE_TYPE> cache_item;
typedef map<CACHE_TYPE> cache_type;

typedef struct {
  string name;
  string version;
  bool is_dev;
} PACKAGE;

typedef struct {
  USHORT name;
  USHORT search;
  USHORT version;
} MAX_LENGTH;

typedef struct {
  bool init;
  bool fake_http_requests;
  bool show_sub_dependencies;
  vector<string> &alternate_rows;
} DEP_OPTS;

enum alternate_modes {
  VERSION,
  DEPENDENCIES,
  INFO,
  VERSION_CHECK,
};

enum main_modes {
  INSTALL,
  PACKAGES,
  FILTERED,
};

#endif // _TYPES_H
