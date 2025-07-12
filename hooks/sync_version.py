from ruamel.yaml import YAML

if __name__ == "__main__":

    yaml = YAML()
    yaml.preserve_quotes = True

    with open("motion_detection_addon/config.yaml", "r") as f:
        data = yaml.load(f)

    with open("VERSION", "r") as f:
        version = f.read().strip()

    data["version"] = version

    with open("motion_detection_addon/config.yaml", "w") as f:
        yaml.dump(data, f)