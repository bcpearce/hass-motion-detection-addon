#!/usr/bin/with-contenv bashio
# shellcheck shell=bash
set -e

# required args when used as an addon
declare -a args=("$@")
args+=(--hass-url 'http://supervisor')
args+=(--hass-token "$SUPERVISOR_TOKEN")

# required args passed in from the config
args+=(--source-url "$(bashio::config 'url')")
args+=(--hass-entity-id "$(bashio::config 'entity_id')")

# required args if authentication with the video source is needed
if [[ $(bashio::config 'password') != null ]]; then
  args+=(--source-password "$(bashio::config 'password')")
fi
if [[ $(bashio::config 'username') != null ]]; then
  args+=(--source-username "$(bashio::config 'username')")
fi
if [[ $(bashio::config 'token') != null ]]; then
  args+=(--source-token "$(bashio::config 'token')")
fi

if [[ $(bashio::config 'detection_debounce_seconds') != null ]]; then
 args+=(--detection-debounce "$(bashio::config 'detection_debounce_seconds')")
fi

if [[ $(bashio::config 'detection_size') != null ]]; then
 args+=(--detection-size "$(bashio::config 'detection_size')")
fi

echo "${args[@]}"

echo "MotionDetection version $(MotionDetection --version)"
MotionDetection "${args[@]}"
