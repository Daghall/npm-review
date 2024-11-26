#include "debug.h"
#include "types.h"
#include <string>

const  USHORT BUFFER_SIZE = 1024;

vector<string> shell_command(const string command)
{
  vector<string> result;
  char buffer[BUFFER_SIZE];
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

int sync_shell_command(const string command, std::function<void(char*)> callback)
{
  char buffer[BUFFER_SIZE];
  FILE *output = popen(command.c_str(), "r");
  setvbuf(output, buffer, _IOLBF, sizeof(buffer));

  debug("Executing sync \"%s\"\n", command.c_str());

  while (fgets(buffer, sizeof(buffer), output) != NULL) {
    size_t last = strlen(buffer) - 1;
    buffer[last] = '\0';
    callback(buffer);
  }

  return pclose(output);
}
