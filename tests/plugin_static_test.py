from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin" / "mosh.koplugin" / "main.lua"


class PluginStaticTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = PLUGIN.read_text()

    def test_registers_tools_menu_entry(self):
        self.assertIn("registerToMainMenu", self.source)
        self.assertIn("menu_items.mosh_client", self.source)

    def test_installs_owned_terminal_shim_atomically(self):
        self.assertIn("# Managed by koreader-mosh", self.source)
        self.assertIn('DataStorage:getDataDir(), "scripts", "mosh"', self.source)
        self.assertIn('paths.shim .. ".tmp"', self.source)
        self.assertIn("os.rename(tmp, paths.shim)", self.source)

    def test_preserves_unrelated_user_scripts(self):
        self.assertIn('state == "foreign"', self.source)
        self.assertIn("Refusing to overwrite", self.source)
        self.assertIn("Refusing to remove", self.source)

    def test_status_mentions_required_runtime_dependencies(self):
        for term in ["mosh-client", "vt52 terminfo", "dbclient", "Locale"]:
            self.assertIn(term, self.source)


if __name__ == "__main__":
    unittest.main()
