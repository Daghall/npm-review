#include <cstdarg>
#include <regex>
#include <stdio.h>
#include "npm-review.h"

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

string get_major(string semver)
{
  const size_t dot = semver.find(".");
  return semver.substr(0, dot);
}

const USHORT number_width(USHORT number)
{
  USHORT rest = number;
  USHORT i = 0;

  while (rest > 0) {
    rest /= 10;
    ++i;
  }

  return i;
}

const char* alternate_mode_to_string(enum alternate_modes alternate_mode)
{
  switch (alternate_mode) {
    case VERSION:
      return "versions";
    case DEPENDENCIES:
      return "dependencies";
    case INFO:
      return "info";
    case VERSION_CHECK:
      return "version check";
  }
}

bool is_printable(char character)
{
  return character >= 0x20 && character < 0x7F;
}

string escape_slashes(string str)
{
  regex slash ("/");
  return regex_replace(str, slash, "\\/");
}

bool starts_with(string haystack, string needle)
{
  return haystack.rfind(needle, 0) != string::npos;
}
