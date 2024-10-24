#!/usr/bin/env bash

# - Print the word diff with minimal context, in porcelain mode
# - Keep lines that start with +, - or five spaces: diff lines or package names
#   of diffed lines
# - Remove lines that have a colon in the middle, since these are newly
#   installed or deleted packages
# - Extract what's inside the quotes, sans leading tilde or caret
# - Print package name and version for each package that has actually changed
#   version. If a package with trailing comma is added or removed, it will
#   cause false positives.

git_command=${1:-git}

# SCRIPT
$git_command diff --word-diff=porcelain -U0 package.json 2> /dev/null | \
  sed -En '/^([-+]|     )"/p' | \
  grep -v '": "' | \
  sed -E -e 's/.*"[~^]?([^"]+)".*/\1/' | \
  awk '
    {
      a[i++] = $0
    }
    END {
      i = 0;
      while (i < NR) {
        if (a[i + 1] != a[i + 2]) {
          print a[i] " " a[i + 1]
        }
      i += 3
    }
  }'
