"""Refresh the actual v2 QML dependency list used by the Android module."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
ui = root / "src/app/ui"
components = {}
for file in ui.rglob("*.qml"):
    if file.stem in components:
        raise RuntimeError(f"Duplicate component name: {file.stem}")
    components[file.stem] = file
entry = root / "src/android/AndroidMain.qml"
pattern = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\s*\{")
pending = list(pattern.findall(entry.read_text(encoding="utf-8"))) + ["Theme"]
used = set()
while pending:
    name = pending.pop()
    if name in used or name not in components:
        continue
    used.add(name)
    pending.extend(pattern.findall(components[name].read_text(encoding="utf-8")))
lines = ["# Actual v2 QML components, flattened exactly as the v2 resource module.",
         "# Regenerate with scripts/build/mobile-qml-closure.py.", "set(MOBILE_V2_QML_FILES"]
lines += [f'    "${{PROJECT_SOURCE_DIR}}/{components[name].relative_to(root).as_posix()}"' for name in sorted(used)]
lines += [")", "foreach(qml IN LISTS MOBILE_V2_QML_FILES)",
          '    get_filename_component(name "${qml}" NAME)',
          '    set_source_files_properties("${qml}" PROPERTIES QT_RESOURCE_ALIAS "${name}")', "endforeach()",
          'set_source_files_properties("${PROJECT_SOURCE_DIR}/src/app/ui/theme/Theme.qml" PROPERTIES QT_QML_SINGLETON_TYPE TRUE)',
          'file(GLOB MOBILE_V2_ICONS "${PROJECT_SOURCE_DIR}/src/app/ui/resources/icons/*")',
          "foreach(icon IN LISTS MOBILE_V2_ICONS)", '    get_filename_component(name "${icon}" NAME)',
          '    set_source_files_properties("${icon}" PROPERTIES QT_RESOURCE_ALIAS "icons/${name}")', "endforeach()"]
(root / "src/android/V2Ui.cmake").write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Included {len(used)} actual v2 QML components")
