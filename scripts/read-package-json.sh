#!/usr/bin/env sh

file=${1:-package.json}

# SCRIPT
for d in .dependencies .devDependencies; do
  jq $d' | keys[] as $k| "\($k) \(.[$k] | sub("[~^]"; "")) '$d'"' -r 2> /dev/null < $file | \
  awk '{
    if ($2 ~ /^npm:/) {
      gsub("^npm:", "", $2);
      i = index(substr($2, 2), "@");
      alias = substr($2, 1, i);
      version = substr($2, i + 2);
      printf("%s\t%s\t%s\t%s\n", $1, version, $3, alias);
    } else {
      printf("%s\t%s\t%s\n", $1, $2, $3);
    }
  }';
done
