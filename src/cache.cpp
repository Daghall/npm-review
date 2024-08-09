#include <cstdarg>
#include <stdio.h>
#include "cache.h"
#include "debug.h"
#include "npm-review.h"
#include "shell.h"
#include "string.h"
#include "types.h"

cache_type dependency_cache;
cache_type info_cache;
cache_type version_cache;
cache_type npm_search_cache;

CACHES _caches {
  .dependencies = &dependency_cache,
  .info = &info_cache,
  .version = &version_cache,
  .npm_search = &npm_search_cache,
};

cache_type* get_cache(alternate_modes alternate_mode)
{
  switch (alternate_mode) {
    case DEPENDENCIES:
      return &dependency_cache;
    case INFO:
      return &info_cache;
    case VERSION:
    case VERSION_CHECK:
      return &version_cache;
  }
}

vector<string> get_from_cache(string package_name, char* command)
{
  cache_type *cache = &npm_search_cache;
  return get_from_cache(package_name, command, cache, "install");
}

vector<string> get_from_cache(string package_name, char* command, enum alternate_modes alternate_mode)
{
  cache_type *cache = get_cache(alternate_mode);
  return get_from_cache(package_name, command, cache, alternate_mode_to_string(alternate_mode));
}

vector<string> get_from_cache(string package_name, char* command, cache_type* cache, const char* name)
{
  if (!cache) return vector<string>();

  cache_type::iterator cache_data = cache->find(package_name);

  if (cache_data != cache->end()) {
    debug("Cache HIT for \"%s\" (%s)\n", package_name.c_str(), name);
  } else {
    debug("Cache MISS for \"%s\" (%s)\n", package_name.c_str(), name);
    cache_data = cache->insert(cache->begin(), cache_item (package_name, shell_command(command)));
  }

  return cache_data->second;
}

CACHES* get_caches()
{
  return &_caches;
}
