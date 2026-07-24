#!/usr/bin/env python3
import json
import sys


def normalise(value):
    return "" if value in {"", "null", "None", None} else value


def as_int(value):
    if value in {"", "null", "None", None}:
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value)
    return value


def build_feed_config(feeds_json, legacy_url, legacy_entity, legacy_username,
                      legacy_password, legacy_token, legacy_debounce,
                      legacy_size):
    feed_objects = {}

    if normalise(feeds_json) not in {"", "null", "None"}:
        try:
            feeds_data = json.loads(feeds_json)
        except json.JSONDecodeError:
            feeds_data = []

        if isinstance(feeds_data, list):
            for index, feed in enumerate(feeds_data, start=1):
                if not isinstance(feed, dict):
                    continue
                name = feed.get("name") or feed.get("entity_id") or f"feed{index}"
                entry = {}
                url = feed.get("url") or feed.get("sourceUrl")
                if url:
                    entry["sourceUrl"] = url
                entity_id = feed.get("entity_id") or feed.get("hassEntityId")
                if entity_id:
                    entry["hassEntityId"] = entity_id
                username = feed.get("username") or feed.get("sourceUsername")
                if username:
                    entry["sourceUsername"] = username
                password = feed.get("password") or feed.get("sourcePassword")
                if password:
                    entry["sourcePassword"] = password
                token = feed.get("token") or feed.get("sourceToken")
                if token:
                    entry["sourceToken"] = token
                debounce = feed.get("detection_debounce_seconds") or feed.get("detectionDebounce")
                if debounce not in {None, ""}:
                    entry["detectionDebounce"] = as_int(debounce)
                size = feed.get("detection_size") or feed.get("detectionSize")
                if size not in {None, ""}:
                    entry["detectionSize"] = size
                if entry:
                    feed_objects[name] = entry
        elif isinstance(feeds_data, dict):
            for name, feed in feeds_data.items():
                if not isinstance(feed, dict):
                    continue
                entry = {}
                url = feed.get("url") or feed.get("sourceUrl")
                if url:
                    entry["sourceUrl"] = url
                entity_id = feed.get("entity_id") or feed.get("hassEntityId")
                if entity_id:
                    entry["hassEntityId"] = entity_id
                username = feed.get("username") or feed.get("sourceUsername")
                if username:
                    entry["sourceUsername"] = username
                password = feed.get("password") or feed.get("sourcePassword")
                if password:
                    entry["sourcePassword"] = password
                token = feed.get("token") or feed.get("sourceToken")
                if token:
                    entry["sourceToken"] = token
                debounce = feed.get("detection_debounce_seconds") or feed.get("detectionDebounce")
                if debounce not in {None, ""}:
                    entry["detectionDebounce"] = as_int(debounce)
                size = feed.get("detection_size") or feed.get("detectionSize")
                if size not in {None, ""}:
                    entry["detectionSize"] = size
                if entry:
                    feed_objects[name] = entry

    if not feed_objects and normalise(legacy_url):
        entry = {"sourceUrl": legacy_url}
        if legacy_entity:
            entry["hassEntityId"] = legacy_entity
        if legacy_username:
            entry["sourceUsername"] = legacy_username
        if legacy_password:
            entry["sourcePassword"] = legacy_password
        if legacy_token:
            entry["sourceToken"] = legacy_token
        if legacy_debounce not in {"", "null", "None"}:
            entry["detectionDebounce"] = as_int(legacy_debounce)
        if legacy_size not in {"", "null", "None"}:
            entry["detectionSize"] = legacy_size
        feed_name = legacy_entity or "default_feed"
        feed_objects[feed_name] = entry

    return json.dumps(feed_objects)


if __name__ == "__main__":
    print(build_feed_config(*sys.argv[1:9]))
