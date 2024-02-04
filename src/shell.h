#ifndef _SHELL_H
#define _SHELL_H

#include <string>
#include <vector>

using namespace std;

int sync_shell_command(const string command, std::function<void(char*)> callback);
vector<string> shell_command(const string command);

#endif // _SHELL_H
