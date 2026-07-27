#!/usr/bin/with-contenv bashio
# shellcheck shell=bash
set -e

# required args when used as an addon
declare -a args=("$@")
args+=(--hass-url 'http://supervisor')
args+=(--hass-token "$SUPERVISOR_TOKEN")
args+=(--source-config "/data/options.json")

echo "MotionDetection version $(MotionDetection --version)"
MotionDetection "${args[@]}"
