#!/usr/bin/env bash

git describe --tags --contains --match 'nightly-*' >&- 2>&-
if [[ $? -eq 0 ]]; then
  # build not needed : latest 'nightly-*' tag points at HEAD
  exit 1
else
  # build needed : output the build timestamp and exit 0
  echo "$(date +%Y-%m-%d-%H-%M-%S)"
  exit 0
fi
