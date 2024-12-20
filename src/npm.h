#ifndef _NPM_H
#define _NPM_H

#include "search.h"
#include "types.h"

void read_packages(MAX_LENGTH *max_length, VIEW &view);
// TODO: Move fake_http_requests to VIEW?
vector<string> get_versions(PACKAGE package, bool fake_http_requests, alternate_modes alternate_mode);
void get_dependencies(VIEW &view, bool init, bool fake_http_requests);
void get_info(PACKAGE package, VIEW &view, bool fake_http_requests);
void install_package(VIEW &view, bool install_dev_dependency);
void install_package(PACKAGE &package, const string new_version, VIEW &view, bool install_dev_dependency);
void uninstall_package(VIEW &view);
bool revert_package(PACKAGE &package, VIEW &view);
void search_for_package(MAX_LENGTH &max_length, VIEW &view, bool fake_http_requests);
void abort_install(VIEW &view);
PACKAGE find_package(vector<PACKAGE> &pkgs, const string &name);

#endif // _NPM_H
