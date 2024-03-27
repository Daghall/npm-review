#!/usr/bin/env sh

package=$1

# SCRIPT
npm info $package versions --json | jq 'if (type == "array") then reverse | .[] else . end' -r
