#ifndef _TYPES_H
#define _TYPES_H

#include <string>

using namespace std;

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

#endif // _TYPES_H
