#!/usr/bin/env sh

package=$1
version=$(
  jq -r '
    .dependencies["'$package'"] //
    .devDependencies["'$package'"]
  ' package.json | \
  sed \
    -e 's/^[~^]//'
  )

# The <BR> stuff is to have the exact same output when running the
# script standalone as compiled into the binary, since newlines are
# removed in the Makefile.
#
# When getting info in the install mode, the current version is not available,
# so it is hidden.

# TODO: Handle invalid installations. For example:
# datadog-metrics               0.11.0 invalid: "~0.11.1" from the root project

# SCRIPT
npm info $package --json 2> /dev/null | \
 jq -r '"
\(.name) | \(.license.type? // .license)<BR>
\(.description)<BR><BR>

\(
  if "'$version'" != "null" then
    "CURRENT<BR>
    '$version' (\(.time["'$version'"] | split("T") | .[0]))<BR><BR>"
  else
    ""
  end
)

LATEST<BR>
\(.version) (\(.time[.version] | split("T") | .[0]))<BR><BR>

DEPENDENCIES<BR>
\(.dependencies // {" ": 0} | keys_unsorted | join("<BR>"))<BR><BR>

HOMEPAGE<BR>
\(.homepage)<BR><BR>

AUTHOR<BR>
\(.author)<BR><BR>

KEYWORDS<BR>
\(.keywords // ["-"] | join(", "))<BR>
"' | \
  tr -d '\n' | \
  sed -E 's/<BR>/\n/g' | \
  sed -E '/DEPENDENCIES/,/^$/s/^([^A-Z]+)$/– &/'
