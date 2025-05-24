#ifndef TEST_RUNNER_CONSTANTS_H
#define TEST_RUNNER_CONSTANTS_H

#include <string>
#include <vector>

#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define RESET   "\x1b[0m"

#define FAILED  RED   "✗" RESET
#define PASSED  GREEN "✓" RESET

#endif // TEST_RUNNER_CONSTANTS_H
