#!/bin/bash
# BINTRAY_USER
# BINTRAY_API_KEY
BINTRAY_REPO=tsMuxer
PCK_NAME=tsMuxerGUI-Nightly
repo_commit=$(curl -s https://dl.bintray.com/$BINTRAY_USER/$BINTRAY_REPO/commit.txt)
local_commit=$(git rev-parse HEAD)
version_date=$(date +%Y-%m-%d--%H-%M-%S)

if [ "$repo_commit" = "$local_commit" ] ; then
  echo "latest nightly build already in bintray!"
  exit 1
else
  # create a new version with the timestamp
  version_data='{ "name": "'
  version_data+=$version_date
  version_data+='", "desc": "tsMuxer CLI and GUI binaries built on '
  version_data+=$version_date
  version_data+='"}'
  echo $version_data
  version_created=$(curl -u$BINTRAY_USER:$BINTRAY_API_KEY -H "Content-Type: application/json" --write-out %{http_code} --silent --output /dev/null --request POST --data "$version_data" https://api.bintray.com/packages/$BINTRAY_USER/$BINTRAY_REPO/$PCK_NAME/versions)
  if [ $version_created -eq 201 ] ; then
    echo "version $version_date has been created!"
    
    # upload the ZIP files to the version we just created on bintray
    echo "uploading files..."
    curl -T ./bin/mac.zip -u$BINTRAY_USER:$BINTRAY_API_KEY -H "X-Bintray-Package:$PCK_NAME" -H "X-Bintray-Version:$version_date"  -H "X-Bintray-Publish:1"  -H "X-Bintray-Override:1" https://api.bintray.com/content/$BINTRAY_USER/$BINTRAY_REPO/mac-nightly-$version_date.zip
    curl -T ./bin/w32.zip -u$BINTRAY_USER:$BINTRAY_API_KEY -H "X-Bintray-Package:$PCK_NAME" -H "X-Bintray-Version:$version_date"  -H "X-Bintray-Publish:1"  -H "X-Bintray-Override:1" https://api.bintray.com/content/$BINTRAY_USER/$BINTRAY_REPO/w32-nightly-$version_date.zip
    curl -T ./bin/w64.zip -u$BINTRAY_USER:$BINTRAY_API_KEY -H "X-Bintray-Package:$PCK_NAME" -H "X-Bintray-Version:$version_date"  -H "X-Bintray-Publish:1"  -H "X-Bintray-Override:1" https://api.bintray.com/content/$BINTRAY_USER/$BINTRAY_REPO/w64-nightly-$version_date.zip
    curl -T ./bin/lnx.zip -u$BINTRAY_USER:$BINTRAY_API_KEY -H "X-Bintray-Package:$PCK_NAME" -H "X-Bintray-Version:$version_date"  -H "X-Bintray-Publish:1"  -H "X-Bintray-Override:1" https://api.bintray.com/content/$BINTRAY_USER/$BINTRAY_REPO/lnx-nightly-$version_date.zip
    echo "files uploaded!"
    
    # update the latest commit on bintray
    echo "updating commit record on bintray..."
    echo $local_commit > commit.txt
    curl -T commit.txt -u$BINTRAY_USER:$BINTRAY_API_KEY -H "X-Bintray-Package:commit" -H "X-Bintray-Version:latest"  -H "X-Bintray-Publish:1"  -H "X-Bintray-Override:1" https://api.bintray.com/content/$BINTRAY_USER/$BINTRAY_REPO/commit.txt
    rm -f commit.txt
    echo "commit record updated!"
  else
    echo "error creating version $version_date !"
    exit 2
  fi
fi
