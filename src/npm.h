#ifndef _NPM_H
#define _NPM_H

#include "search.h"
#include "types.h"

const USHORT COMMAND_SIZE = 1024;

void read_packages(MAX_LENGTH *max_length, vector<PACKAGE> *pkgs);
vector<string> get_versions(PACKAGE package, bool fake_http_requests, alternate_modes alternate_mode);
void get_dependencies(PACKAGE package, vector<string> &alternate_rows, short &selected_alternate_row, bool init, bool show_sub_dependencies, bool fake_http_requests);
void get_info(PACKAGE package, vector<string> &alternate_rows, short &selected_alternate_row, bool fake_http_requests, main_modes main_mode);
void install_package(PACKAGE &package, const string new_version, short &selected_alternate_row, vector<PACKAGE> &pkgs, bool install_dev_dependency);
void uninstall_package(PACKAGE package, vector<PACKAGE> &pkgs);
void search_for_package(MAX_LENGTH &max_length, vector<PACKAGE> &filtered_packages, Search *searching, USHORT &selected_package, bool fake_http_requests);
void abort_install(main_modes &main_mode, USHORT &selected_package);

#endif // _NPM_H
