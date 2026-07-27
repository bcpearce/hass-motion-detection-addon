from ruamel.yaml import YAML
import re

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

    with open("motion_detection_addon/Dockerfile", encoding="utf-8", mode="r") as f:
        addon_dockerfile = f.read()
        new_addon_dockerfile = re.sub(
            r"MOTION_DETECTION_VERSION=\d\.\d\.\d",
            f"MOTION_DETECTION_VERSION={version}",
            addon_dockerfile,
        )

    with open("motion_detection_addon/Dockerfile", encoding="utf-8", mode="w") as f:
        f.write(new_addon_dockerfile)
