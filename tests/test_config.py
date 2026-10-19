"""Run with the Python environment containing ESPHome and PyYAML."""

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]


class ConfigTests(unittest.TestCase):
    def validate(self, config):
        config = copy.deepcopy(config)
        config["external_components"][0]["source"]["path"] = str(ROOT / "components")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.yaml"
            path.write_text(yaml.safe_dump(config, allow_unicode=True))
            result = subprocess.run(
                [sys.executable, "-m", "esphome", "config", str(path)],
                capture_output=True,
                text=True,
            )
        return result.returncode, result.stdout + result.stderr

    def test_minimal_and_full(self):
        for name in ("minimal", "full"):
            with self.subTest(name=name):
                code, output = self.validate(
                    yaml.safe_load((ROOT / f"tests/{name}.yaml").read_text())
                )
                self.assertEqual(code, 0, output)

    def test_existing_entities_preserved(self):
        manifest = json.loads((ROOT / "tests/entity_manifest.json").read_text())
        for path in ("packages/sensia-arena.yaml", "tests/full.yaml"):
            config = yaml.safe_load((ROOT / path).read_text())
            for platform, expected in manifest.items():
                actual = [
                    {
                        key: value
                        for key, value in item.items()
                        if key
                        in (
                            "name",
                            "id",
                            "entity_category",
                            "disabled_by_default",
                            "device_class",
                        )
                    }
                    for item in config[platform]
                ]
                self.assertEqual(actual, expected, (path, platform))

    def test_invalid_configurations(self):
        base = yaml.safe_load((ROOT / "tests/full.yaml").read_text())
        cases = [
            ("esp32_ble", "name", "Other", "requires esp32_ble name"),
            ("esp32_ble", "auth_req_mode", "no_bond", "auth_req_mode: bond"),
            ("esp32_ble_server", "max_clients", 2, "one BLE client"),
            ("sensia_arena", "initial_settings", 7, "bit 0"),
            ("sensia_arena", "initial_settings", 256, "255"),
            ("sensia_arena", "pairing_enabled", True, "either pairing_enabled"),
        ]
        for section, key, value, message in cases:
            with self.subTest(key=key, value=value):
                config = copy.deepcopy(base)
                config[section][key] = value
                if key == "max_clients":
                    config["esp32_ble"]["max_connections"] = 2
                code, output = self.validate(config)
                self.assertNotEqual(code, 0)
                self.assertIn(message, output)
        for platform, key, value in [
            ("switch", "inverted", True),
            ("switch", "restore_mode", "ALWAYS_ON"),
        ]:
            config = copy.deepcopy(base)
            config[platform][0][key] = value
            self.assertNotEqual(self.validate(config)[0], 0)
        config = copy.deepcopy(base)
        config["esp32_ble_server"]["services"] = [
            {
                "uuid": "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
                "characteristics": [
                    {"uuid": "6e400002-b5a3-f393-e0a9-e50e24dcca9e", "write": True}
                ],
            }
        ]
        code, output = self.validate(config)
        self.assertNotEqual(code, 0)
        self.assertIn("creates its own service", output)
        config = copy.deepcopy(base)
        del config["sensia_arena"]["time_id"]
        self.assertNotEqual(self.validate(config)[0], 0)
        config = copy.deepcopy(base)
        config["number"][0]["type"] = "INVALID"
        self.assertNotEqual(self.validate(config)[0], 0)


if __name__ == "__main__":
    unittest.main()
