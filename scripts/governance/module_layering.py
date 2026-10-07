"""Check the source module layering (Python standard library only).

The library table mirrors docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md:
every product source directory belongs to exactly one CMake library, a
library may only include headers of the libraries it is allowed to depend on
(transitively), third-party and Qt private headers stay inside their owners,
and the CMake link lines declared in cmake/MiaCodeModules.cmake agree with the
same table. Run from anywhere: ``python scripts/governance/module_layering.py``.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# name -> (source directories, allowed direct dependencies)
LIBRARIES = {
    "miacode_base": (["src/common"], []),
    "miacode_chart": (["src/core/chart"], ["miacode_base"]),
    "miacode_analysis": (["src/core/analysis"], ["miacode_chart"]),
    "miacode_editor_core": (["src/editor"], ["miacode_chart"]),
    "miacode_scene": (["src/core/scene", "src/core/video"], ["miacode_analysis"]),
    "miacode_audio": (["src/audio"], ["miacode_scene"]),
    "miacode_audio_bass": (["src/audio/bass"], ["miacode_audio"]),
    "miacode_timeline": (["src/timeline"], ["miacode_analysis", "miacode_audio"]),
    "miacode_timeline_quick": (["src/timeline/quick"], ["miacode_timeline"]),
    "miacode_preview_quick": (["src/preview/quick_scene", "src/preview/runtime"], ["miacode_scene"]),
    "miacode_stage_media": (["src/preview/stage_media"], ["miacode_scene", "miacode_preview_quick"]),
    "miacode_export": (["src/export"], ["*"]),
    "miacode_media_tools": (["src/media_tools"], ["miacode_chart"]),
    "MiaCode": (["src/app"], ["*"]),
}
APP = "MiaCode"
# Directories that hold no product library code.
UNOWNED = ["src/tools", "src/devtools", "src/wrapper", "src/intro", "src/extensions"]

# (header pattern, libraries allowed to include it, description)
RESTRICTED_INCLUDES = [
    (re.compile(r'^(bass|bassmix|bass_fx|bassflac|bassopus|bass_aac)\.h$'),
     {"miacode_audio_bass", "miacode_export"}, "BASS"),
    (re.compile(r'^QtAVPlayer/'), {"miacode_stage_media", "miacode_export"}, "QtAVPlayer/FFmpeg"),
    (re.compile(r'^(libav\w+|libsw\w+)/'), {"miacode_stage_media", "miacode_export"}, "FFmpeg"),
    (re.compile(r'^scintillaquick/'), {APP}, "ScintillaQuick"),
    (re.compile(r'^miniz\.h$'), {"miacode_media_tools", APP}, "miniz"),
    (re.compile(r'^(QtNetwork/|QNetwork\w*$|QSslError$|QHttp\w*$)'), {"miacode_media_tools", APP}, "QtNetwork"),
    (re.compile(r'(_p\.h$|/private/)'), {"miacode_stage_media", "miacode_export"}, "Qt private API"),
    (re.compile(r'^(QtMultimedia/|QVideoFrame$|QVideoSink$|QMediaPlayer$|QMediaDevices$|QAudio\w*$|QSoundEffect$)'),
     {"miacode_audio", "miacode_audio_bass", "miacode_preview_quick", "miacode_stage_media",
      "miacode_export", APP}, "QtMultimedia"),
]

# App-internal direction: runtime and services sit below ui; nothing below the
# process entry includes the entry-point header.
APP_RULES = [
    (("src/app/runtime/", "src/app/services/"), "src/app/ui/", "app runtime/services must not include app/ui"),
    (("src/app/runtime/", "src/app/services/", "src/app/ui/", "src/app/quick_shell/", "src/app/platform/"),
     "src/app/MainEntrypoints.h", "only process entry points include MainEntrypoints.h"),
]

SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".c", ".inc", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)
MODULE_FILE = "cmake/MiaCodeModules.cmake"


def owner_of(path: str):
    best, best_len = None, -1
    for name, (dirs, _) in LIBRARIES.items():
        for d in dirs:
            if (path == d or path.startswith(d + "/")) and len(d) > best_len:
                best, best_len = name, len(d)
    return best


def allowed_closure(name: str) -> set[str]:
    seen: set[str] = set()
    stack = list(LIBRARIES[name][1])
    while stack:
        dep = stack.pop()
        if dep == "*":
            seen.update(n for n in LIBRARIES if n not in (APP, name))
            continue
        if dep not in seen:
            seen.add(dep)
            stack.extend(LIBRARIES[dep][1])
    return seen


def source_files(root: Path):
    for path in sorted((root / "src").rglob("*")):
        if path.is_file() and path.suffix in SOURCE_SUFFIXES:
            yield path.relative_to(root).as_posix()


def check_sources(root: Path) -> list[str]:
    errors: list[str] = []
    files = list(source_files(root))
    known = set(files)
    for rel in files:
        if any(rel == d or rel.startswith(d + "/") for d in UNOWNED):
            continue
        owner = owner_of(rel)
        if owner is None:
            errors.append(f"{rel}: directory belongs to no library")
            continue
        allowed = allowed_closure(owner) | {owner}
        text = (root / rel).read_text(encoding="utf-8", errors="replace")
        for match in INCLUDE.finditer(text):
            kind, header = match.groups()
            line = text.count("\n", 0, match.start()) + 1
            target = "src/" + header
            if kind == '"' and target in known:
                dep = owner_of(target)
                if dep is not None and dep not in allowed and not any(
                        target.startswith(d + "/") for d in UNOWNED):
                    errors.append(f"{rel}:{line}: {owner} must not include {header} ({dep})")
                for sources, forbidden, message in APP_RULES:
                    if rel.startswith(sources) and (target == forbidden or target.startswith(forbidden)):
                        errors.append(f"{rel}:{line}: {message}")
                continue
            if kind == '"' and "/" not in header and (root / rel).parent.joinpath(header).exists():
                errors.append(f"{rel}:{line}: include \"{header}\" by its src-rooted path")
                continue
            for pattern, owners, label in RESTRICTED_INCLUDES:
                if pattern.search(header) and owner not in owners:
                    errors.append(f"{rel}:{line}: {label} header <{header}> is not allowed in {owner}")
    return errors


def parse_modules(root: Path):
    """Return {target: {"deps": set, "sources": [..]}} from miacode_add_module calls."""
    path = root / MODULE_FILE
    if not path.exists():
        return None
    text = re.sub(r"#[^\n]*", "", path.read_text(encoding="utf-8"))
    modules = {}
    for match in re.finditer(r"miacode_add_module\s*\(\s*(\w+)(.*?)\)\s*$", text, re.S | re.M):
        name, body = match.group(1), match.group(2)
        tokens = body.split()
        section, deps, sources = None, set(), []
        for token in tokens:
            if token in ("SOURCES", "PUBLIC", "PRIVATE", "QT", "RESOURCES", "DIRS"):
                section = token
                continue
            if section in ("PUBLIC", "PRIVATE") and token.startswith("miacode_"):
                deps.add(token)
            elif section == "SOURCES":
                sources.append(token)
        modules[name] = {"deps": deps, "sources": sources}
    return modules


def check_cmake(root: Path) -> list[str]:
    modules = parse_modules(root)
    if modules is None:
        return [f"{MODULE_FILE} is missing"]
    errors = []
    for name in LIBRARIES:
        if name != APP and name not in modules:
            errors.append(f"{MODULE_FILE}: library {name} is not declared")
    for name, info in modules.items():
        if name not in LIBRARIES:
            errors.append(f"{MODULE_FILE}: {name} is not in the layering table")
            continue
        allowed = allowed_closure(name)
        for dep in sorted(info["deps"]):
            if dep not in allowed:
                errors.append(f"{MODULE_FILE}: {name} links {dep}, which its layer may not depend on")
        for source in info["sources"]:
            if source.startswith("${") or not source.startswith("src/"):
                continue
            if owner_of(source) != name:
                errors.append(f"{MODULE_FILE}: {name} lists {source}, owned by {owner_of(source)}")
            if not (root / source).exists():
                errors.append(f"{MODULE_FILE}: {name} lists missing {source}")
    listed = {s for info in modules.values() for s in info["sources"]}
    for rel in source_files(root):
        owner = owner_of(rel)
        if owner in (None, APP) or any(rel.startswith(d + "/") for d in UNOWNED):
            continue
        if rel not in listed:
            errors.append(f"{rel}: not listed in {owner} sources")
    return errors


def run(root: Path = ROOT) -> list[str]:
    return check_sources(root) + check_cmake(root)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args(argv)
    errors = run(args.root)
    for error in errors:
        print(error)
    if errors:
        print(f"{len(errors)} layering violation(s).")
        return 1
    print("Module layering is consistent.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
