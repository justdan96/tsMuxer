#!/usr/bin/env bash

git describe --tags --contains --match 'nightly-*' >&- 2>&-
if [[ $? -eq 0 ]]; then
  # build not needed : latest 'nightly-*' tag points at HEAD
  exit 1
else
  # build needed : output the desired tag name and exit 0
  echo "nightly-$(date +%Y-%m-%d-%H-%M-%S)"
  exit 0
fi
