from pathlib import Path
from typing import Any

import toml  # type: ignore


class ConfigBuilder:
    def __init__(self, filepath: Path) -> None:
        self._filepath = filepath
        if self._filepath.exists():
            self._data = toml.load(self._filepath)
        else:
            self._data = {}

    def set_value(self, key: str, value: Any) -> None:
        """
        Use dotted keys to set toml values - For example:

        "https.enable_https"

        Maps to:

        [https]
        use_https = ...
        """
        keys = key.split(".")
        data = self._data
        for key in keys[:-1]:
            if key not in data or not isinstance(data[key], dict):
                data[key] = {}
            data = data[key]
        data[keys[-1]] = value

    def save(self) -> None:
        with open(self._filepath, "w") as f:
            toml.dump(self._data, f)
