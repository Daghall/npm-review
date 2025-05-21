#include "debug.h"

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

char translate_character(const wchar_t character)
{
  debug("Translating character: %lc (%d)\n", character, character);
  switch (character & 0xFFFF) {
    case L'┬': return '+';
    case L'├': return '+';
    case L'│': return '|';
    case L'└': return '`';
    case L'─': return '-';
  }
  return character;
}

void dump_screen(VIEW &view)
{
  FILE *fp = fopen("/tmp/npm-review.dump", "w");

  if (!fp) {
    show_error_message("Failed to open file for writing");
    return;
  }

  short half_cols = COLS / 2 - 1;
  short max_cols = view.alternate_window ? half_cols : COLS;
  wchar_t c;

  for (int row = 0; row < LIST_HEIGHT; ++row) {
    // Main window
    for (int col = 0; col < max_cols; ++col) {
      c = mvwinch(view.package_window, row + view.start_packages, col);
      debug("Charmain: %c (%d)\n", c & A_CHARTEXT, c);

      if (col == 0 && (c & A_STANDOUT)) {
        fprintf(fp, "#");
      } else {
        fprintf(fp, "%c", translate_character(c));
      }
    }

    if (view.alternate_window) {
      // Separator
      if (view.alternate_window) {
        fprintf(fp, "|");
      }

      // Alternate window
      for (int col = 0; col < COLS - half_cols - 1; ++col) {
        c = mvwinch(view.alternate_window, row + view.start_alternate, col);
        debug("Char alt: %c (%d)\n", c, c);

        if (col == 0 && (c & A_STANDOUT)) {
          fprintf(fp, "#");
        } else {
          fprintf(fp, "%c", translate_character(c));
        }
      }
    }

    fprintf(fp, "\n");
  }

  // Read the info bar from the stdscr window
  for (int row = LIST_HEIGHT; row < LINES; ++row) {
    for (int col = 0; col < COLS; ++col) {
      wchar_t c = mvwinch(stdscr, row, col);
      fprintf(fp, "%c", translate_character(c));
    }
    fprintf(fp, "\n");
  }

  fclose(fp);
}
