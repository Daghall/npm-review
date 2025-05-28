#ifndef TEST_RUNNER_TYPES_H
#define TEST_RUNNER_TYPES_H

#include <string>
#include <vector>
#include <map>

using namespace std;

struct SCENARIO; // Forward declaration

typedef unsigned short USHORT;

typedef struct TEST_RESULT {
  USHORT passed_tests;
  USHORT failed_tests;
} TEST_RESULT;

typedef struct FEATURE {
  string name;
  vector<SCENARIO> scenarios;
} FEATURE;

typedef struct SCENARIO {
  string name;
  vector<string> input;
  vector<string> expected_output;
  FEATURE *feature;
  TEST_RESULT *result;
} SCENARIO;

typedef struct options {
  bool break_on_failure;
  bool no_exit_code;
  string grep_pattern;
} OPTIONS;

#endif // TEST_RUNNER_TYPES_H
