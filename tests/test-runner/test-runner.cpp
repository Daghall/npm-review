#include <map>
#include <regex>
#include <vector>
#include <string>
#include "getopt.h"
#include "constants.h"
#include "types.h"
#include "feature.h"

int exit(TEST_RESULT &result, OPTIONS &options) {
  if (options.no_exit_code) {
    return 0;
  } else {
    return result.failed_tests;
  }
}

int main(int argc, char* argv[]) {
  const char *short_options = "bxg:";
  struct option long_options[] = {
    {"break",           no_argument,        nullptr,  'b'},
    {"grep",            required_argument,  nullptr,  'x'},
    {"no-exit-code",    no_argument,        nullptr,  'x'},
    {nullptr,           0,                  nullptr,  0}
  };

  OPTIONS options = {
    .break_on_failure = false,
    .no_exit_code = false,
  };

  char opt;
  while ((opt = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
    switch (opt) {
      case 'b': // --break
        options.break_on_failure = true;
        break;
      case 'x': // --no-exit-code
        options.no_exit_code = true;
        break;
      case 'g': // --grep
        options.grep_pattern = optarg;
        break;
      default:
        fprintf(stderr, "\nUsage: %s [OPTIONS]\n\n", argv[0]);
        fprintf(stderr, "Options:\n"
          "  -b, --break           Break on first failure\n"
          "  -g, --grep            Filter features/scenarios by name (regex)\n"
          "  -x, --no-exit-code    Do not return an exit code (always 0)\n"
        );
        return 1;
    }
  }

  TEST_RESULT result = {
    .passed_tests = 0,
    .failed_tests = 0,
  };

  const string test_dir = "./tests/tui/";
  map<string, vector<string>> test_files = read_features(test_dir);
  vector<FEATURE> features = parse_features(test_files, result);

  if (!options.grep_pattern.empty()) {
    regex grep_regex(options.grep_pattern, regex_constants::icase | regex_constants::extended);

    vector<FEATURE> filtered_features;
    for (const FEATURE& feature : features) {
      if (regex_search(feature.name, grep_regex)) {
        filtered_features.push_back(feature);
      } else {
        vector<SCENARIO> filtered_scenarios;

        for (const SCENARIO& scenario : feature.scenarios) {
          if (regex_search(scenario.name, grep_regex)) {
            filtered_scenarios.push_back(scenario);
          }
        }

        if (!filtered_scenarios.empty()) {
          FEATURE filtered_feature = feature;
          filtered_feature.scenarios = filtered_scenarios;
          filtered_features.push_back(filtered_feature);
        }
      }
    }
    features = filtered_features;
  }

  for (const FEATURE& feature : features) {
    const bool success = run_feature(feature, options);

    if (!success && options.break_on_failure) {
      return exit(result, options);
    }
  }

  USHORT max_number = max(result.passed_tests, result.failed_tests);
  USHORT max_width = to_string(max_number).length();
  printf("\n  %s%s Passed: %*hu%s\n", PASSED, GREEN, max_width, result.passed_tests, RESET);

  if (result.failed_tests > 0) {
    printf("  %s%s Failed: %*hu%s\n", FAILED, RED, max_width, result.failed_tests, RESET);
  }

  printf("\n");

  return exit(result, options);
}
