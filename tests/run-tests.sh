#!/usr/bin/env bash

# Declare tests
declare -A tests
tests["diff-versions"]="./scripts/diff-versions.sh ./tests/mocks/git-diff.sh"
tests["read-package-json"]="./scripts/read-package-json.sh ./tests/read-package-json.input"


# Determine diff program to use when pretty-printing
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

# Execute tests
for test in "${!tests[@]}"; do
    output=/tmp/test_$test.actual
    # echo $output
    rm $output 2> /dev/null
    ${tests[$test]} > $output
    res=$(diff -u $output tests/$test.expected)

  if [[ $? -ne 0 ]]; then
    printf "\x1b[31m· %-24s ✗\x1b[0m\n" $test
    print_diff "$res"
  else
    printf "\x1b[32m· %-24s ✔\x1b[0m\n" $test
  fi 
done
