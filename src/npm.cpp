// vi: fdm=marker
#include <regex>
#include "cache.h"
#include "debug.h"
#include "git.h"
#include "npm.h"
#include "search.h"
#include "shell.h"
#include "string.h"
#include "types.h"
#include "npm-review.h"

const char *DEPENDENCIES_STRING = {
#include "../build/dependencies"
};

const char *INFO_STRING = {
#include "../build/info"
};

const char *VERSIONS_STRING = {
#include "../build/versions"
};

const char *NPM_SEARCH_STRING = {
#include "../build/npm-search"
};

CACHES *caches = get_caches();

void read_packages(MAX_LENGTH *max_length, VIEW &view)
{
  debug("Reading packages\n");
  string command = "for dep in .dependencies .devDependencies; do jq $dep' | keys[] as $key | \"\\($key) \\(.[$key] | sub(\"[~^]\"; \"\")) '$dep'\"' -r 2> /dev/null < package.json; done";
  vector<string> packages = shell_command(command);
  view.pkgs.clear();

  version_type diff_versions = get_diff_versions();

  for_each(packages.begin(), packages.end(), [&max_length, &view, &diff_versions](string &package) {
    const vector<string> strings = split_string(package);
    string name = strings.at(0);
    string version = strings.at(1);
    string origin = strings.at(2);

    const version_type::iterator version_item = diff_versions.find(name);
    const string original_version = version_item != diff_versions.end() ? version_item->second : "";
    debug("Package: %s %s \t\t\t%s\n", name.c_str(), version.c_str(), original_version.c_str());

    view.pkgs.push_back({
      .name = name,
      .version = version,
      .original_version = original_version,
      .is_dev = origin == ".devDependencies"
    });

    if (max_length && name.length() > max_length->name) {
      max_length->name = name.length();
    }
    if (max_length && version.length() > max_length->version) {
      max_length->version = version.length();
    }
  });
}

vector<string> get_versions(PACKAGE package, bool fake_http_requests, alternate_modes alternate_mode)
{
  if (fake_http_requests) { // {{{1
    render_alternate_window_border();
    vector<string> fake_response = vector<string>();
    fake_response.push_back("5.1.1");
    fake_response.push_back("5.1.0");
    fake_response.push_back("5.0.0");
    fake_response.push_back("4.2.0");
    fake_response.push_back("4.1.3");
    fake_response.push_back("4.1.2");
    fake_response.push_back("4.1.1");
    fake_response.push_back("4.1.0");
    fake_response.push_back("4.0.0");
    fake_response.push_back("3.7.0");
    fake_response.push_back("3.6.0");
    fake_response.push_back("3.5.0");
    fake_response.push_back("3.4.0");
    fake_response.push_back("3.3.0");
    fake_response.push_back("3.2.0");
    fake_response.push_back("3.1.0");
    fake_response.push_back("3.0.0");
    fake_response.push_back("2.4.0");
    fake_response.push_back("2.3.0");
    fake_response.push_back("2.2.0");
    fake_response.push_back("2.1.0");
    fake_response.push_back("2.0.1");
    fake_response.push_back("2.0.0");
    fake_response.push_back("1.2.2");
    fake_response.push_back("1.2.1");
    fake_response.push_back("1.2.0");
    fake_response.push_back("1.0.0");
    return fake_response;
  } else { // }}}
    char command[COMMAND_SIZE];
    snprintf(command, COMMAND_SIZE, VERSIONS_STRING, package.name.c_str());
    if (alternate_mode != VERSION_CHECK) {
      init_alternate_window();
    }

    return get_from_cache(package.name, command, alternate_mode);
  }
}

void get_dependencies(VIEW &view, bool init, bool fake_http_requests)
{
  PACKAGE package = view.filtered_packages.at(view.selected_package);
  string package_name = escape_slashes(package.name);
  string selected;

  if (!init) {
    selected = find_dependency_root();
  }

  if (fake_http_requests) { // {{{1
    render_alternate_window_border();
    view.alternate_rows.clear();
    if (!view.show_sub_dependencies) {
      view.alternate_rows.push_back("┬ express-handlebars    5.3.5");
      view.alternate_rows.push_back("├─┬ glob                7.2.0");
      view.alternate_rows.push_back("├── graceful-fs         4.2.10");
      view.alternate_rows.push_back("└─┬ handlebars          4.7.7");
    } else {
      view.alternate_rows.push_back("┬ express-handlebars    5.3.5");
      view.alternate_rows.push_back("├─┬ glob                7.2.0");
      view.alternate_rows.push_back("│ ├── fs.realpath       1.0.0");
      view.alternate_rows.push_back("│ ├─┬ inflight          1.0.6");
      view.alternate_rows.push_back("│ │ ├── once            1.4.0");
      view.alternate_rows.push_back("│ │ └── wrappy          1.0.2");
      view.alternate_rows.push_back("│ ├── inherits          2.0.4");
      view.alternate_rows.push_back("│ ├── minimatch         3.1.2");
      view.alternate_rows.push_back("│ ├─┬ once              1.4.0");
      view.alternate_rows.push_back("│ │ └── wrappy          1.0.2");
      view.alternate_rows.push_back("│ └── path-is-absolute  1.0.1");
      view.alternate_rows.push_back("├── graceful-fs         4.2.10");
      view.alternate_rows.push_back("└─┬ handlebars          4.7.7");
      view.alternate_rows.push_back("  ├── minimist          1.2.6");
      view.alternate_rows.push_back("  ├── neo-async         2.6.2");
      view.alternate_rows.push_back("  ├── source-map        0.6.1");
      view.alternate_rows.push_back("  ├── uglify-js         3.15.3");
      view.alternate_rows.push_back("  └── wordwrap          1.0.0");
    }
  } else { // }}}
    char command[COMMAND_SIZE];
    snprintf(command, COMMAND_SIZE, DEPENDENCIES_STRING, package_name.c_str(), "^$");
    init_alternate_window();
    vector<string> dependency_data = get_from_cache(package_name, command, DEPENDENCIES);

    if (view.show_sub_dependencies) {
      view.alternate_rows = dependency_data;
    } else {
      regex sub_dependency_regex ("^(│| )");
      view.alternate_rows.clear();
      copy_if(dependency_data.begin(), dependency_data.end(), back_inserter(view.alternate_rows), [&sub_dependency_regex](string dependency) {
        return !regex_search(dependency, sub_dependency_regex);
      });
    }
  }

  view.selected_alternate_row = 0;
  select_dependency_node(selected);
  print_alternate(&package);
}

void get_info(PACKAGE package, VIEW &view, bool fake_http_requests)
{
  string package_name = escape_slashes(package.name);
  const char* package_version = package.version.c_str();

  if (fake_http_requests) { // {{{1
    render_alternate_window_border();
    view.alternate_rows.clear();
    view.alternate_rows.push_back("express-handlebars | BSD-3-Clause");
    view.alternate_rows.push_back("A Handlebars view engine for Express which doesn't suck.");
    view.alternate_rows.push_back("");
    view.alternate_rows.push_back("CURRENT");
    view.alternate_rows.push_back("5.3.5 (2021-11-13)");
    view.alternate_rows.push_back("");
    view.alternate_rows.push_back("LATEST");
    view.alternate_rows.push_back("7.0.7 (2023-04-15)");
    view.alternate_rows.push_back("");
    view.alternate_rows.push_back("DEPENDENCIES");
    view.alternate_rows.push_back("– glob");
    view.alternate_rows.push_back("– graceful-fs");
    view.alternate_rows.push_back("– handlebars");
    view.alternate_rows.push_back("");
    view.alternate_rows.push_back("HOMEPAGE");
    view.alternate_rows.push_back("https://github.com/express-handlebars/express-handlebars");
    view.alternate_rows.push_back("");
    view.alternate_rows.push_back("AUTHOR");
    view.alternate_rows.push_back("Eric Ferraiuolo <eferraiuolo@gmail.com> (http://ericf.me/)");
    view.alternate_rows.push_back("");
    view.alternate_rows.push_back("KEYWORDS");
    view.alternate_rows.push_back("express, express3, handlebars, view, layout, partials, templates");
  } else { // }}}
    char command[COMMAND_SIZE];
    const char* version = view.main_mode == INSTALL ? "null" : package_version;
    snprintf(command, COMMAND_SIZE, INFO_STRING, package_name.c_str(), version, version, version);
    init_alternate_window();
    view.alternate_rows = get_from_cache(package_name, command, INFO);
  }
  view.selected_alternate_row = 0;
  print_alternate(&package);
}

void install_package(VIEW &view, bool install_dev_dependency)
{
  PACKAGE package = view.filtered_packages.at(view.selected_package);
  string new_version = view.alternate_rows.at(view.selected_alternate_row);
  install_package(package, new_version, view, install_dev_dependency);
}

void install_package(PACKAGE &package, const string new_version, VIEW &view, bool install_dev_dependency)
{
  show_message("Installing...");

  string package_name = escape_slashes(package.name);

  caches->dependencies->erase(package_name);
  caches->info->erase(package_name);

  char command[COMMAND_SIZE];
  snprintf(command, COMMAND_SIZE, "npm install %s@%s --silent %s", package.name.c_str(), new_version.c_str(), install_dev_dependency ? "--save-dev" : "--save");
  int exit_code = sync_shell_command(command, [](char* line) {
    debug("NPM INSTALL: %s\n", line);
  });

  hide_cursor();

  if (exit_code == OK) {
    debug("Installed %s@%s %s\n", package.name.c_str(), new_version.c_str(), install_dev_dependency ? "(DEV)" : "");
    show_message("Installed " + package.name + "@" + new_version + (install_dev_dependency ? " (DEV)" : ""));
    package.version = new_version;
    read_packages(nullptr, view);
    view.selected_alternate_row = -1;

    if (get_alternate_window()) {
      PACKAGE updated_package = find_package(view.pkgs, package.name);
      print_alternate(&updated_package);
    }
  } else {
    show_error_message("Installation failed");
    debug("Install failed\n");
  }
}

bool revert_package(PACKAGE &package, VIEW &view)
{
  if (view.alternate_window && view.alternate_mode != VERSION) return false;
  if (package.original_version == "") {
    show_message("Already at oldest change");
    return false;
  }

  if (confirm("Revert " + package.name + " to " + package.original_version + "?")) {
    install_package(package, package.original_version, view, package.is_dev);
    return true;
  }

  return false;
}

void uninstall_package(VIEW &view)
{
  PACKAGE package = view.filtered_packages.at(view.selected_package);

  if (!confirm("Uninstall " + package.name + "?")) return;

  show_message("Uninstalling...");

  char command[COMMAND_SIZE];
  snprintf(command, COMMAND_SIZE, "npm uninstall %s --silent", package.name.c_str());

  int exit_code = sync_shell_command(command, [](char* line) {
    debug("NPM UNINSTALL: %s\n", line);
  });

  hide_cursor();

  if (exit_code == OK) {
    debug("Package uninstalled\n");
    show_message("Uninstalled " + package.name);
    read_packages(nullptr, view);
  } else {
    debug("Uninstall failed: %d\n", exit_code);
    show_error_message("Uninstall failed");
  }
}

void search_for_package(MAX_LENGTH &max_length, VIEW &view, bool fake_http_requests)
{
  char command[COMMAND_SIZE];
  const string host = fake_http_requests ? "http://localhost:3000" : "https://api.npms.io";
  debug("NPM_SEARCH_STRING=%s\n", NPM_SEARCH_STRING);
  if (view.searching->is_search_mode() && view.searching->package.string.length() > 0) {
    view.filtered_packages.clear();
    snprintf(command, COMMAND_SIZE, NPM_SEARCH_STRING, "-q", host.c_str(), to_string(LIST_HEIGHT).c_str(), view.searching->package.string.c_str());
    debug("Get string: %s\n", command);
    vector<string> s = get_from_cache(view.searching->package.string, command);
    vector<string>::iterator it = s.begin();

    max_length.search = 0;

    // NOTE: The total is printed by the script, but not used, so the first
    // item is skipped by the initial `++it`

    while (++it != s.end()) {
      debug("Install search: %s | %s \n", (*it).c_str(), (*(it + 1)).c_str());
      max_length.search = max(max_length.search, (USHORT)(*it).length());
      view.filtered_packages.push_back({
        .name = *it,
        .version = *(++it),
      });
    }
    view.selected_package = 0;
  }
}

void abort_install(VIEW &view)
{
  if (view.main_mode == INSTALL) {
    view.main_mode = PACKAGES;
    show_message("Install aborted");
    view.selected_package = 0;
  }
}

PACKAGE find_package(vector<PACKAGE> &pkgs, const string &name)
{
  return *find_if(pkgs.begin(), pkgs.end(), [&name](PACKAGE pkg) {
    return pkg.name == name;
  });
}
