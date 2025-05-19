#!/usr/bin/env bash

exit_code=0

# Declare tests
declare -A tests
tests["diff-versions"]="./scripts/diff-versions.sh ./tests/mocks/git-diff.sh"
tests["read-package-json"]="./scripts/read-package-json.sh ./tests/read-package-json.input"
tests["dependencies"]="./scripts/dependencies.sh ./tests/mocks/npm-dependencies.sh %s"


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

# Run a single test
function run_test() {
  test_name=$1
  cmd=$2

  output=/tmp/test_$test_name.actual
  rm $output 2> /dev/null
  $cmd > $output
  res=$(diff -u $output tests/$test_name.expected)

  if [[ $? -ne 0 ]]; then
    exit_code=$((exit_code + 1))
    printf "\x1b[31m· %-30s ✗\x1b[0m\n" $test_name
    print_diff "$res"
  else
    printf "\x1b[32m· %-30s ✔\x1b[0m\n" $test_name
  fi
}

# Execute all tests
for test in "${!tests[@]}"; do
  if [[ -f "tests/$test.variations" ]]; then
    sed 's/ *#.*//g' tests/$test.variations | \
      while read -r variation; do
        test_cmd=$(printf "${tests[$test]}\n" $variation)
        run_test "$test-$variation" "$test_cmd"
      done
  else
    run_test "$test" "${tests[$test]}"
  fi
done

exit $exit_code
