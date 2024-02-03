#include <cstdarg>
#include <stdio.h>

FILE *debug_log = NULL;

void init_logging()
{
  debug_log = fopen("./log", "w");
}

void debug(const char *format, ...)
{
  if (!debug_log) return;

  va_list args;
  va_start(args, format);

  vfprintf(debug_log, format, args);
  fflush(debug_log);

  va_end(args);
}

void debug_key(const char character, const char *window_name)
{
  debug("Sending key  %s%c\t(%#04x) to %s\n",
   (character < 0x1f ? "^" : ""),
   (character < 0x1f ? character + 0x60 : character),
   character,
   window_name
  );
}

void terminate_logging()
{
  fclose(debug_log);
}
