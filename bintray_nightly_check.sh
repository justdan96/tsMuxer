#!/usr/bin/env bash

# Checks if the commit hash of the current newest nightly build on
# Bintray is the same as the current HEAD, in which case returns a
# nonzero exit code. Otherwise, tries to create a new "version" in the
# Bintray repository.

# This script should be used as the entry point of "build nightly"
# actions in order to prevent unnecessarily triggering builds if nothing
# has changed.

# BINTRAY_USER and BINTRAY_API_KEY must be present in the environment.

version_date=$(date +%Y-%m-%d)

create_version() {
  local package_name=$1
  local vcs_ver=$2

  # create a new version with the timestamp
  version_data='{ "name": "'
  version_data+=$version_date
  version_data+='", "desc": "tsMuxer CLI and GUI binaries built on '
  version_data+=$version_date
  version_data+="\", \"vcs_tag\": \"${vcs_ver}\"}"
  echo "$version_data"

  version_created=$(curl -u$BINTRAY_USER:$BINTRAY_API_KEY -H "Content-Type: application/json" --write-out %{http_code} --silent --output /dev/null --request POST --data "$version_data" https://api.bintray.com/packages/$BINTRAY_USER/$BINTRAY_REPO/${package_name}/versions)
  if [[ $version_created -eq 201 ]]; then
    echo "version $version_date has been created!"
  else
    echo "error creating version $version_date : server returned $version_created"
  fi
}

BINTRAY_REPO=tsMuxer
PCK_NAME=tsMuxerGUI-Nightly
repo_commit=$(curl -s https://api.bintray.com/packages/$BINTRAY_USER/$BINTRAY_REPO/$PCK_NAME/versions/_latest | jq -r .vcs_tag)
local_commit=$(git rev-parse HEAD)

if [[ $repo_commit == $local_commit ]]; then
  echo "latest nightly build already in bintray!"
  exit 1
fi

create_version "$PCK_NAME" "$local_commit"

exit 0 # don't return an error in case the version already exists
