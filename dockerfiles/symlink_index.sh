#!/bin/bash

set -e
set -o pipefail

# Docker definitely materializes symlinks when using `COPY --from...`.
# Also it sometimes does it in
# layered inheritance (haven't figured the exact circumstances).
# This utility is a generic remedy for that. Inside a directory,
# call the `build` command to list all symlinks recursively together with
# source files that they are pointing to in a local file (index).
# Then call `restore` to make everything back according to the index.
# A blunt uniform way to use it would be doing `restore rebuild`
# as the last statement of a docker layer.

if [ "$#" == "0" ]; then
  echo >&2 "Symlink index: command not specified, available commands:"
  echo >&2 "  build print rm restore"
  exit 1
fi
for cmd in "$@"; do
  if [ "$cmd" == "build" ]; then
    rm -rf __symlinks__.txt
    find -L . -xtype l | while read -r line; do
      echo "$(pwd)/${line:2}:$(readlink -f "$line")" >>__symlinks__.txt
    done
  elif [ "$cmd" == "print" ]; then
    cat __symlinks__.txt
  elif [ "$cmd" == "rm" ]; then
    rm -rf __symlinks__.txt
  elif [ "$cmd" == "restore" ]; then
    if [ ! -e __symlinks__.txt ]; then
      continue
    fi
    while read -r line; do
      link_file=${line/:*/}
      src_file=${line/*:/}
      if [ ! -L "$link_file" ]; then
        echo "Symlink index: restoring symlink $line"
        if [ ! -e "$src_file" ]; then
          echo "Symlink index: source file doesn't exist anymore"
        else
          ln -s -f "$src_file" "$link_file"
        fi
      fi
    done <__symlinks__.txt
  else
    echo >&2 "Symlink index: unknown command: $cmd"
    exit 1
  fi
done
