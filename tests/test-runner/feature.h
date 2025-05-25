#ifndef FEATURE_H
#define FEATURE_H

#include "scenario.h"
#include "types.h"

void run_feature(FEATURE feature);
map<string, vector<string>> read_features(const string& folder);
vector<FEATURE> parse_features(map<string, vector<string>> &test_files, TEST_RESULT &result);

#endif // FEATURE_H
