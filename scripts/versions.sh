#!/usr/bin/env bash

if [[ $# -gt 1 ]]; then
  npm_binary=$1
  shift
else
  npm_binary=npm
fi

package=$1

# SCRIPT
$npm_binary info $package versions --json | jq 'if (type == "array") then reverse | .[] else . end' -r
