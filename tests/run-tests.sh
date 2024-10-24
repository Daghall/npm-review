#!/usr/bin/env bash


# Determine diff program
which delta >> /dev/null
if [[ $? -eq 0 ]]; then
  diff_cmd="delta --color-only --paging=never"
else
  diff_cmd="cat"
fi

# Print diff with colors
function print_diff() {
  echo "$1" | \
  $diff_cmd | \
  sed \
    -e 's/^-/\x1b[31m-/g' \
    -e 's/^+/\x1b[32m+/g' \
    -e 's/$/\x1b[0m/g'
}

# diff-versions.sh
./scripts/diff-versions.sh ./tests/mocks/git-diff.sh > /tmp/npm-review-version.actual

res=$(diff -u tests/diff-versions.expected /tmp/npm-review-version.actual)

if [ $? -ne 0 ]; then
  echo -e "\x1b[31mTest failed\x1b[0m"
  print_diff "$res"
else
  echo -e "\x1b[32mTest passed\x1b[0m"
fi

