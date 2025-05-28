#ifndef FEATURE_H
#define FEATURE_H

#include "scenario.h"
#include "types.h"

bool run_feature(FEATURE feature, OPTIONS options);
map<string, vector<string>> read_features(const string& folder);
vector<FEATURE> parse_features(map<string, vector<string>> &test_files, TEST_RESULT &result);

#endif // FEATURE_H
