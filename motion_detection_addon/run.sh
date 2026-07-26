#!/usr/bin/with-contenv bashio
# shellcheck shell=bash
set -e

# required args when used as an addon
declare -a args=("$@")
args+=(--hass-url 'http://supervisor')
args+=(--hass-token "$SUPERVISOR_TOKEN")

read_config_value() {
  local key="$1"
  local value
  value="$(bashio::config "$key")"
  if [[ "$value" == "null" ]]; then
    echo ""
  else
    echo "$value"
  fi
}

build_feed_config() {
  local feeds_json="$1"
  local legacy_url="$2"
  local legacy_entity="$3"
  local legacy_username="$4"
  local legacy_password="$5"
  local legacy_token="$6"
  local legacy_debounce="$7"
  local legacy_size="$8"
  local feed_config_script="${FEED_CONFIG_SCRIPT:-/build_feed_config.py}"

  if [[ ! -f "$feed_config_script" ]]; then
    feed_config_script="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/build_feed_config.py"
  fi

  python3 "$feed_config_script" "$feeds_json" "$legacy_url" "$legacy_entity" "$legacy_username" \
    "$legacy_password" "$legacy_token" "$legacy_debounce" "$legacy_size"
}

feeds_config="$(read_config_value 'feeds')"
legacy_url="$(read_config_value 'url')"
legacy_entity="$(read_config_value 'entity_id')"
legacy_username="$(read_config_value 'username')"
legacy_password="$(read_config_value 'password')"
legacy_token="$(read_config_value 'token')"
legacy_debounce="$(read_config_value 'detection_debounce_seconds')"
legacy_size="$(read_config_value 'detection_size')"

feed_config_json="$(build_feed_config "$feeds_config" "$legacy_url" "$legacy_entity" "$legacy_username" "$legacy_password" "$legacy_token" "$legacy_debounce" "$legacy_size")"
if [[ -n "$feed_config_json" && "$feed_config_json" != '{}' ]]; then
  args+=(--source-config-raw "$feed_config_json")
fi

echo "${args[@]}"

echo "MotionDetection version $(MotionDetection --version)"
MotionDetection "${args[@]}"
