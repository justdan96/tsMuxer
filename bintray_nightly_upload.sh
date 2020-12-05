#!/usr/bin/env bash
# BINTRAY_USER
# BINTRAY_API_KEY

BINTRAY_REPO=tsMuxer
PCK_NAME=tsMuxerGUI-Nightly
version_date=$(date +%Y-%m-%d)

upload_to_bintray() {
  local localpath="$1"
  local buildname="$2"
  curl -T "$localpath" "-u$BINTRAY_USER:$BINTRAY_API_KEY" \
    -H "X-Bintray-Package:$PCK_NAME" \
    -H "X-Bintray-Version:$version_date" \
    -H "X-Bintray-Publish:1" \
    -H "X-Bintray-Override:1" \
    "https://api.bintray.com/content/$BINTRAY_USER/$BINTRAY_REPO/${buildname}-nightly-$version_date.zip"
}

# when triggered with arguments, just upload the given file and exit.
if [[ "$@" ]]; then
  upload_to_bintray "$@"
  exit 0
fi

# upload the ZIP files to the version we just created on bintray
echo "uploading files..."
for buildname in w32 w64 lnx; do
  upload_to_bintray "./bin/${buildname}.zip" "$buildname"
done
echo "files uploaded!"

# update the latest commit on bintray
echo "updating commit record on bintray..."
git rev-parse HEAD | curl -T - "-u$BINTRAY_USER:$BINTRAY_API_KEY" -H "X-Bintray-Package:commit" -H "X-Bintray-Version:latest"  -H "X-Bintray-Publish:1"  -H "X-Bintray-Override:1" "https://api.bintray.com/content/$BINTRAY_USER/$BINTRAY_REPO/commit.txt"0
echo "commit record updated!"

# try to trigger a new build in OBS
obs_trigger=$(curl --user $OBS_USER:$OBS_SECRET --write-out %{http_code} --silent --output /dev/null -H 'Accept-Encoding: identity' -H 'User-agent: osc/0.164.2' -H 'Content-type: application/x-www-form-urlencoded' -X POST https://api.opensuse.org/build/home:justdan96?cmd=rebuild&package=tsMuxer)
echo $obs_trigger
#if [ $obs_trigger -eq 200 ] ; then
#  echo "build has been triggered in OBS!"
#else
#  echo "error triggering build in OBS!"
#  exit 3
#fi
