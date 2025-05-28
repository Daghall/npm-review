#ifndef SCENARIO_H
#define SCENARIO_H

#include "types.h"

bool run_scenario(SCENARIO scenario, OPTIONS options);
vector<string> read_result();
void dump_screen(int master_fd);
void flush_buffer(int master_fd);
void spawn_child();

#endif // SCENARIO_H
