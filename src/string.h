#ifndef _STRING_H
#define _STRING_H

#include <string>
#include "npm-review.h"

vector<std::string> split_string(string package_string);
string get_major(string semver);
const USHORT number_width(USHORT number);
const char* alternate_mode_to_string(enum alternate_modes alternate_mode);
bool is_printable(char character);
string escape_slashes(string str);
bool starts_with(string haystack, string needle);

#endif // _STRING_H
