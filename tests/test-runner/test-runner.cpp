#include <map>
#include <vector>
#include <string>
#include "constants.h"
#include "types.h"
#include "feature.h"

int main() {
  // TODO: Support running only one Feature (or Scenario), if specified in argv
  // TODO: Add break on failure, so that the test runner stops on the first failure

  TEST_RESULT result = {
    .passed_tests = 0,
    .failed_tests = 0,
  };

  const string test_dir = "./tests/tui/";
  map<string, vector<string>> test_files = read_features(test_dir);
  vector<FEATURE> features = parse_features(test_files, result);

  for (const FEATURE& feature : features) {
    run_feature(feature);
  }

  USHORT max_number = max(result.passed_tests, result.failed_tests);
  USHORT max_width = to_string(max_number).length();
  printf("\n  %s%s Passed: %*hu%s\n", PASSED, GREEN, max_width, result.passed_tests, RESET);

  if (result.failed_tests > 0) {
    printf("  %s%s Failed: %*hu%s\n", FAILED, RED, max_width, result.failed_tests, RESET);
  }
}
