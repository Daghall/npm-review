#ifndef _DEBUG_H
#define _DEBUG_H

void init_logging();
void debug(const char *format, ...);
void debug_key(const char character, const char *window_name);
void terminate_logging();

#endif // _DEBUG_H
