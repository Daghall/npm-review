#!/usr/bin/env sh

package=$1
pattern="${2:-^$}"

# SCRIPT
npm ls --all 2> /dev/null | \
  sed 's/ UNMET DEPENDENCY/ !/' | \
  sed 's/ UNMET OPTIONAL DEPENDENCY/ !⎇/' | \
  sed -n "/^... $package@/,/^[├└]/p" | \
  sed -E \
    -e 's/..// ' \
    -e 's/ deduped//' \
    -e 's/@/\t/g' \
    -e 's/(([─┬]) )/\2\t/g' \
    -e 's/ \t/@/g' | \
  tr -s '\t' | \
  grep -v "$pattern" | \
  gawk -F '\t' '\
    BEGIN {max = 0} \
    { \
      len = length($1) + length($2); \
      max = len > max ? len : max; \
      data[FNR][0] = $1; \
      data[FNR][1] = $2; \
      data[FNR][2] = $3 \
    } \
    END { \
    for (i = 1; i < NR; ++i) { \
      printf("%s %-*s  %s\n", \
        data[i][0], max - length(data[i][0]), \
        data[i][1], \
        data[i][2]); \
      } \
    }'
