#!/usr/bin/with-contenv bashio
# shellcheck shell=bash
set -e

# required args when used as an addon
declare -a args=("$@")
args+=(--hass-url 'http://supervisor')
args+=(--hass-token "$SUPERVISOR_TOKEN")
RAWCONF=$(/data/options.json | jq '.feeds | map({(.name): .}) | add')
args+=(--source-config-raw $RAWCONF)

echo "MotionDetection version $(MotionDetection --version)"
MotionDetection "${args[@]}"
