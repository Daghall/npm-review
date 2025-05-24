#ifndef TEST_RUNNER_TYPES_H
#define TEST_RUNNER_TYPES_H

#include <string>
#include <vector>
#include <map>

using namespace std;

struct SCENARIO; // Forward declaration, to avoid circular dependencies

typedef struct FEATURE {
  string name;
  vector<SCENARIO> scenarios;
} FEATURE;

typedef struct SCENARIO {
  string name;
  vector<string> input;
  vector<string> expected_output;
  FEATURE *feature;
} SCENARIO;

#endif // TEST_RUNNER_TYPES_H
