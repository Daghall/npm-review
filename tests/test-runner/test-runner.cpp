#include <map>
#include <vector>
#include <string>
#include "types.h"
#include "feature.h"

int main() {
  // TODO: Support running only one Feature (or Scenario), if specified in argv
  // TODO: Add break on failure, so that the test runner stops on the first failure

  const string test_dir = "./tests/tui/";
  map<string, vector<string>> test_files = read_features(test_dir);
  vector<FEATURE> features = parse_features(test_files);

  for (const FEATURE& feature : features) {
    run_feature(feature);
  }
}
