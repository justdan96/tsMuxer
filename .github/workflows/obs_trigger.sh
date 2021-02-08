#!/usr/bin/env bash

obs_trigger=$(curl --user $OBS_USER:$OBS_SECRET --write-out %{http_code} --silent --output /dev/null -H 'Accept-Encoding: identity' -H 'User-agent: osc/0.164.2' -H 'Content-type: application/x-www-form-urlencoded' -X POST https://api.opensuse.org/build/home:justdan96?cmd=rebuild&package=tsMuxer)
echo $obs_trigger
