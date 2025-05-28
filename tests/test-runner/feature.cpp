#include <fstream>
#include <filesystem>
#include "feature.h"
#include "scenario.h"
#include "types.h"

namespace fs = filesystem;

bool run_feature(FEATURE feature, OPTIONS options)
{
  printf("  Feature: %s\n", feature.name.c_str());

  for (const SCENARIO &scenario : feature.scenarios) {
     const bool success = run_scenario(scenario, options);

     if (!success && options.break_on_failure) {
       return false;
     }
  }

  return true;
}

map<string, vector<string>> read_features(const string& folder)
{
  map<string, vector<string>> result;

  for (const auto& entry : fs::directory_iterator(folder)) {
    if (entry.is_regular_file() && entry.path().extension() == ".md") {
      const string filename = entry.path().string();
      ifstream file(filename);
      if (!file) {
        printf("Unable to read file: %s\n", filename.c_str());
        continue;
      }

      vector<string> lines;
      string line;
      while (getline(file, line)) {
        lines.push_back(line);
      }

      result[filename] = lines;
    }
  }

  return result;
}

// Crude "parser" for the feature files.
//
// A feature can contain multiple scenarios, each with its own input and
// expected output. Each scenario is defined by a name, input lines, and
// expected output lines.
//
// The files are markdown files with a specific structure:
// # Feature Name
// ## Scenario 1 Name
// ### Input
// ```
// input line 1
// ...
// input line n
// ### Output
// ```
// output line 1
// ...
// output line n
// ```
vector<FEATURE> parse_features(map<string, vector<string>> &test_files, TEST_RESULT &result)
{
  vector<FEATURE> features;

  for (const auto& pair : test_files) {
    const vector<string> lines = pair.second;
    vector<string> *variable_pointer = nullptr;
    FEATURE feature = {};
    bool read;

    for (const string &line : lines) {
      if (line.empty()) {
        continue;
      }

      if (line == "```") {
        read = !read;
        if (!read && variable_pointer != nullptr) {
          variable_pointer = nullptr;
          continue;
        }
      } else if (variable_pointer != nullptr) {
        variable_pointer->push_back(line);
      } else if (line[0] == '#') {
        USHORT index = line.find(" ");
        switch (index) {
        case 1:
          feature.name = line.substr(2);
          break;
        case 2:
          feature.scenarios.push_back({
            .name = line.substr(3),
            .input = {},
            .expected_output = {},
            .feature = &feature,
            .result = &result,
          });
          read = false;
          break;
        case 3:
          if (line.substr(4) == "Input") {
            variable_pointer = &feature.scenarios.back().input;
          } else if (line.substr(4) == "Output") {
            variable_pointer = &feature.scenarios.back().expected_output;
          } else {
            // TODO: Throw error?
            // TODO: Add environment variables
          }
        }
      }
    }

    features.push_back(feature);
  }

  return features;
}
