#!/usr/bin/env sh

query=$1

flags="-q"
host="https://api.npms.io"
size=$(tput lines)

if [[ -n $DEBUG ]]; then
  echo "Query: $query"
  flags=""
  host="http://localhost:3000"
fi


# SCRIPT
json=$(wget $flags "$host/v2/search/suggestions?size=$size&q=$query" -O -);
total=$(jq length <<< ${json});

echo ${total};
jq -r '.[].package | "\(.name)\n\(.description)"' <<< ${json} 2>/dev/null
