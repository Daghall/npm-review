#define _XOPEN_SOURCE 600
#include <unistd.h>
#include <util.h>
#include <filesystem>
#include <fstream>
#include "constants.h"
#include "types.h"
#include "scenario.h"

namespace fs = filesystem;

void run_scenario(SCENARIO scenario)
{
  int master_fd;
  pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);

  if (pid == -1) {
    perror("forkpty");
    exit(1);
  }

  if (pid == 0) {
    spawn_child();
  } else {
    char scenario_name[256];
    snprintf(scenario_name, sizeof scenario_name, "%%s    %%s Scenario: %s", scenario.name.c_str());
    printf(scenario_name, "", "-");

    // Write to PTY and flush the input
    for_each(scenario.input.begin(), scenario.input.end(), [&master_fd](const string& line) {
      write(master_fd, line.c_str(), line.length());
    });
    dump_screen(master_fd);
    write(master_fd, "Q", 1);
    flush_buffer(master_fd);

    // Check the result
    bool success = true;
    vector<string> result = read_result();
    vector<string>::iterator result_it = result.begin();
    vector<string>::iterator expected_it = scenario.expected_output.begin();

    while (result_it != result.end() && expected_it != scenario.expected_output.end()) {
      if (*result_it != *expected_it) {
        USHORT index = result_it - result.begin() + 1;
        if (success) {
          fflush(stdout);
          success = false;
        }
        printf(scenario_name, "\r", FAILED);
        printf("\n%s    Missmatch at line %hu:\n", YELLOW, index);
        printf(  "%s    Expected: \"%s\"\n", GREEN, expected_it->c_str());
        printf(  "%s    Actual:   \"%s\"%s", RED, result_it->c_str(), RESET);
      }
      ++result_it;
      ++expected_it;
    }

    if (result.size() != scenario.expected_output.size()) {
      if (success) {
        printf(scenario_name, "\r", FAILED);
      }
      success = false;
      printf("\n%s    Missmatch in length:\n", YELLOW);
      printf(  "%s    Expected: %2zu\n", GREEN, scenario.expected_output.size());
      printf(  "%s    Actual:   %2zu\n", RED, result.size());
    }

    if (success) {
      printf("\r");
      printf(scenario_name, "\r", PASSED);
      ++scenario.result->passed_tests;
    } else {
      fs::rename("/tmp/npm-review.dump", "/tmp/npm-review_dump_" + scenario.feature->name + "__" + scenario.name + ".dump");
      ++scenario.result->failed_tests;
    }

    printf("\n%s", RESET);
    close(master_fd);
  }
}

vector<string> read_result()
{
  const string filename = "/tmp/npm-review.dump";
  ifstream file(filename);
  vector<string> result;
  string row;

  while (getline(file, row)) {
    result.push_back(row);
  }

  return result;
}

void dump_screen(int master_fd)
{
  char ctrl_underscore = '_' & 0x1F;
  write(master_fd, &ctrl_underscore, 1);
}

// The pseudo-terminal (PTY) is a pair of devices: the master and the slave.
// If the slave is not read, the master will block when writing to it.
void flush_buffer(int master_fd)
{
  char buf[1024];
  while ((read(master_fd, buf, sizeof(buf) - 1)) > 0);
}

void spawn_child()
{
  // TODO: make these configurable?
  setenv("LINES", "11", 1);
  setenv("COLUMNS", "50", 1);
  execl("./npm-review", "npm-review", "-f", "-c",
        "./tests/scripts/read-package-json.input", NULL);
  perror("execl");
  exit(1);
}
