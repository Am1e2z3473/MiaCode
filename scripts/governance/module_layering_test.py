"""Behavioral checks for the module layering script (Python standard library)."""
from pathlib import Path
import tempfile
import unittest

import module_layering

MODULES = """
miacode_add_module(miacode_base
    SOURCES
        src/common/Log.h
    PUBLIC Qt6::Core
)
miacode_add_module(miacode_chart
    SOURCES
        src/core/chart/Chart.h
    PUBLIC miacode_base
)
"""


class LayeringTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.libraries = module_layering.LIBRARIES
        module_layering.LIBRARIES = {
            "miacode_base": (["src/common"], []),
            "miacode_chart": (["src/core/chart"], ["miacode_base"]),
            "MiaCode": (["src/app"], ["*"]),
        }
        self.addCleanup(setattr, module_layering, "LIBRARIES", self.libraries)
        self.write("src/common/Log.h", "#pragma once\n")
        self.write("src/core/chart/Chart.h", '#pragma once\n#include "common/Log.h"\n')
        self.write("src/app/main.cpp", '#include "core/chart/Chart.h"\n')
        self.write(module_layering.MODULE_FILE, MODULES)

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def errors(self):
        return module_layering.run(self.root)

    def assert_error(self, phrase):
        errors = self.errors()
        self.assertTrue(any(phrase in e for e in errors), errors)

    def test_clean_tree_passes(self):
        self.assertEqual(self.errors(), [])

    def test_upward_include_fails(self):
        self.write("src/common/Log.h", '#pragma once\n#include "core/chart/Chart.h"\n')
        self.assert_error("miacode_base must not include core/chart/Chart.h")

    def test_library_must_not_reach_app(self):
        self.write("src/app/Settings.h", "#pragma once\n")
        self.write("src/core/chart/Chart.h", '#pragma once\n#include "app/Settings.h"\n')
        self.assert_error("must not include app/Settings.h")

    def test_bare_sibling_include_fails(self):
        self.write("src/core/chart/Other.h", "#pragma once\n")
        self.write("src/core/chart/Chart.h", '#pragma once\n#include "Other.h"\n')
        self.assert_error('include "Other.h" by its src-rooted path')

    def test_restricted_third_party_header_fails(self):
        self.write("src/core/chart/Chart.h", '#pragma once\n#include "bass.h"\n')
        self.assert_error("BASS header <bass.h> is not allowed in miacode_chart")

    def test_qt_private_header_fails_outside_owners(self):
        self.write("src/core/chart/Chart.h", "#pragma once\n#include <QtQuick/private/qquickitem_p.h>\n")
        self.assert_error("Qt private API")

    def test_cmake_link_against_layering_fails(self):
        self.write(module_layering.MODULE_FILE, MODULES.replace("PUBLIC Qt6::Core", "PUBLIC miacode_chart"))
        self.assert_error("miacode_base links miacode_chart")

    def test_unlisted_library_source_fails(self):
        self.write("src/core/chart/Extra.cpp", '#include "core/chart/Chart.h"\n')
        self.assert_error("src/core/chart/Extra.cpp: not listed in miacode_chart sources")

    def test_source_listed_in_wrong_library_fails(self):
        self.write(module_layering.MODULE_FILE, MODULES.replace(
            "src/core/chart/Chart.h", "src/core/chart/Chart.h\n        src/common/Log.h"))
        self.assert_error("miacode_chart lists src/common/Log.h, owned by miacode_base")

    def test_app_runtime_must_not_include_ui(self):
        self.write("src/app/ui/View.h", "#pragma once\n")
        self.write("src/app/runtime/Host.cpp", '#include "app/ui/View.h"\n')
        self.assert_error("app runtime/services must not include app/ui")

    def test_entry_header_only_for_entry_points(self):
        self.write("src/app/MainEntrypoints.h", "#pragma once\n")
        self.write("src/app/ui/Window.cpp", '#include "app/MainEntrypoints.h"\n')
        self.assert_error("only process entry points include MainEntrypoints.h")


if __name__ == "__main__":
    unittest.main()
