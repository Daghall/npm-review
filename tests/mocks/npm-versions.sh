#!/usr/bin/env bash

# The ouput must be JSON compatible

if [[ "$2" == "single" ]]; then
  echo '"1.2.3"'
else
  echo '["1.2.3", "1.2.4", "1.2.5"]'
fi
