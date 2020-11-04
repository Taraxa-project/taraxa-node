#!/bin/bash

# reads files from stdin, spawns a parallel subprocess for each file.
# uses presence of <replacement> tag in the output as failure indicator

pids=""
while read -r line; do
  (
    replacement_count=$(
      clang-format \
        -style=file -fallback-style=none -output-replacements-xml "$line" |
        grep -c "<replacement "
    )
    if [ "$replacement_count" -ne 0 ]; then
      echo >&2 "ERROR: unformatted file found: $line"
      exit 1
    fi
  ) &
  pids+="$! "
done <&0
exit_with=0
for pid in $pids; do
  if ! wait "$pid"; then
    exit_with=1
  fi
done
exit $exit_with
