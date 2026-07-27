#!/usr/bin/with-contenv bashio
# shellcheck shell=bash
set -e

# required args when used as an addon
declare -a args=("$@")
args+=(--hass-url 'http://supervisor')
args+=(--hass-token "$SUPERVISOR_TOKEN")
cat /data/options.json | jq '.feeds | map({(.name): .}) | add' > feeds.json
args+=(--source-config feeds.json)

echo "MotionDetection version $(MotionDetection --version)"
MotionDetection "${args[@]}"
