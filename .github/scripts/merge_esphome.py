import re
import yaml
import os
import sys
from pathlib import Path
from copy import deepcopy

BASE_DIR = Path(__file__).parent


# --- Tag wrapper: preserves !secret and !lambda as real YAML tags ---
class ESPHomeTag:
    def __init__(self, tag, value):
        self.tag = tag
        self.value = value


# --- Custom YAML loader ---
class ESPHomeLoader(yaml.SafeLoader):
    pass

def construct_include(loader, node):
    include_path = BASE_DIR / loader.construct_scalar(node)
    with open(include_path) as f:
        return yaml.load(f, Loader=ESPHomeLoader)

def construct_secret(loader, node):
    # Preserve as a real YAML tag so ESPHome Device Builder resolves it from secrets.yaml
    return ESPHomeTag("!secret", loader.construct_scalar(node))

def construct_preserved_tag(loader, tag_suffix, node):
    # Preserve !lambda and other tags as real YAML tags
    return ESPHomeTag(f"!{tag_suffix}", loader.construct_scalar(node))

ESPHomeLoader.add_constructor("!include", construct_include)
ESPHomeLoader.add_constructor("!secret", construct_secret)
ESPHomeLoader.add_multi_constructor("!", construct_preserved_tag)


# --- Custom YAML dumper ---
class ESPHomeDumper(yaml.Dumper):
    pass

def represent_esphome_tag(dumper, data):
    # !secret: plain scalar — post_process() strips the forced quotes pyyaml adds
    # !lambda and others: double-quoted value so the C++ code is unambiguous
    style = '' if data.tag == '!secret' else '"'
    return dumper.represent_scalar(data.tag, data.value, style=style)

def represent_str(dumper, data):
    needs_quotes = (
        '${' in data                          # ESPHome substitution variables
        or not data                           # empty string
        or any(ord(c) > 0x7F for c in data)  # non-ASCII / icon font glyphs
    )
    style = '"' if needs_quotes else None
    return dumper.represent_scalar('tag:yaml.org,2002:str', data, style=style)

ESPHomeDumper.add_representer(ESPHomeTag, represent_esphome_tag)
ESPHomeDumper.add_representer(str, represent_str)


def post_process(output: str) -> str:
    # pyyaml always quotes tagged scalar values; strip quotes for !secret identifiers
    # e.g. !secret 'api_key' or !secret "api_key" → !secret api_key
    return re.sub(r"(!secret) ['\"](\w+)['\"]", r"\1 \2", output)


# --- Deep merge: packages are merged INTO main config ---
def deep_merge(base: dict, override: dict) -> dict:
    result = deepcopy(base)
    for key, val in override.items():
        if key in result:
            if isinstance(result[key], dict) and isinstance(val, dict):
                result[key] = deep_merge(result[key], val)
            elif isinstance(result[key], list) and isinstance(val, list):
                result[key] = val + result[key]  # packages have lower priority
            else:
                # main config wins
                pass
        else:
            result[key] = deepcopy(val)
    return result


def resolve(main_yaml_path: Path) -> dict:
    global BASE_DIR
    BASE_DIR = main_yaml_path.parent
    with open(main_yaml_path) as f:
        config = yaml.load(f, Loader=ESPHomeLoader)

    packages = config.pop("packages", {})
    merged = {}

    for pkg_name, pkg_data in packages.items():
        if isinstance(pkg_data, dict):
            merged = deep_merge(merged, pkg_data)
        # remote github:// packages are skipped (not resolvable offline)

    # Main config wins over packages
    final = deep_merge(merged, config)
    use_github_components(final)
    use_github_images(final)
    return final


GITHUB_COMPONENTS_SOURCE = "github://TheDiffi/cyd-jukebox@master"
GITHUB_RAW_ESPHOME_DIR = "https://raw.githubusercontent.com/TheDiffi/cyd-jukebox/master/esphome/"


def use_github_components(config: dict) -> None:
    # The FULL file is used on its own (e.g. in ESPHome Device Builder), where the repo's
    # components/ folder isn't next to it, so pull local components from GitHub instead
    for entry in config.get("external_components", []):
        source = entry.get("source")
        if isinstance(source, dict) and source.get("type") == "local":
            entry["source"] = GITHUB_COMPONENTS_SOURCE


def use_github_images(config: dict) -> None:
    # Same for the icon PNGs in esphome/icons/: load them from the repo instead
    for entry in config.get("image", []):
        file = entry.get("file")
        if isinstance(file, str) and file.startswith("icons/"):
            entry["file"] = {"type": "web", "url": GITHUB_RAW_ESPHOME_DIR + file}


def main():
    input_file = Path(sys.argv[1])
    output_file = Path(sys.argv[2])

    output_file.parent.mkdir(parents=True, exist_ok=True)
    resolved = resolve(input_file)

    output = yaml.dump(resolved, Dumper=ESPHomeDumper, default_flow_style=False, allow_unicode=True, sort_keys=False)
    output = post_process(output)

    with open(output_file, "w") as f:
        f.write(output)

    print(f"✅  Merged → {output_file}")

if __name__ == "__main__":
    main()
