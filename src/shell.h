#ifndef _SHELL_H
#define _SHELL_H

#include <functional>
#include <string>
#include <vector>
#include "types.h"

using namespace std;

const USHORT COMMAND_SIZE = 1024;

int sync_shell_command(const string command, std::function<void(char*)> callback);
vector<string> shell_command(const string command);

#endif // _SHELL_H
