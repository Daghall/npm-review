#include "debug.h"
#include "shell.h"
#include "string.h"
#include "types.h"

const char *DIFF_VERSIONS = {
#include "../build/diff-versions"
};

version_type get_diff_versions() {
  char command[COMMAND_SIZE];
  snprintf(command, COMMAND_SIZE, DIFF_VERSIONS, "git");
  vector<string> version_data = shell_command(command);

  version_type version_items;

  for_each(version_data.begin(), version_data.end(), [&version_items](string &version) {
    const vector<string> strings = split_string(version);
    string name = strings.at(0);
    string semver = strings.at(1);
    version_items[name] = semver;
  });

  for_each(version_items.begin(), version_items.end(), [](version_item version_item) {
    debug("Original version: %s %s\n", version_item.first.c_str(), version_item.second.c_str());
  });

  return version_items;
}
