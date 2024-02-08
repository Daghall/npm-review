#ifndef _CACHE_H
#define _CACHE_H

#include "types.h"

typedef struct cache_struct {
  cache_type *dependencies;
  cache_type *info;
  cache_type *version;
} CACHES;

cache_type* get_cache(alternate_modes alternate_mode);
vector<string> get_from_cache(string package_name, char* command, enum alternate_modes alternate_mode);
CACHES* get_caches();

#endif // _CACHE_H
