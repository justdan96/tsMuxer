#!/usr/bin/env bash

set -x
set -e

export MACOSX_DEPLOYMENT_TARGET=10.15

brew install freetype

# use Qt5 by default but try to query from environment
if ! qtver=$(qmake -query QT_VERSION | cut -d'.' -f1); then
  qtver=5
fi

source scripts/macos_common_build.sh
