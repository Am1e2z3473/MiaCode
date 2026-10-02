"""Audit packaged native ABI and ELF LOAD alignment; never infer from one .so."""
import argparse
import json
import pathlib
import struct
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("apk", type=pathlib.Path)
parser.add_argument("--output", type=pathlib.Path)
args = parser.parse_args()
libraries = []
with zipfile.ZipFile(args.apk) as apk:
    for entry in apk.infolist():
        if not entry.filename.startswith("lib/") or not entry.filename.endswith(".so"):
            continue
        data = apk.read(entry)
        if data[:6] != b"\x7fELF\x02\x01":
            raise ValueError(f"Expected little-endian ELF64: {entry.filename}")
        machine = struct.unpack_from("<H", data, 18)[0]
        phoff = struct.unpack_from("<Q", data, 32)[0]
        phsize, count = struct.unpack_from("<HH", data, 54)
        loads = []
        for index in range(count):
            kind, flags, offset, address, physical, files, memory, alignment = struct.unpack_from(
                "<IIQQQQQQ", data, phoff + index * phsize)
            if kind == 1:
                loads.append({"alignment": alignment, "congruent16Kb": (address - offset) % 16384 == 0})
        libraries.append({"path": entry.filename, "arm64": machine == 183 and entry.filename.startswith("lib/arm64-v8a/"),
            "loads": loads, "elf16Kb": bool(loads) and all(load["alignment"] >= 16384 and load["congruent16Kb"] for load in loads)})
report = {"schema": 1, "apk": str(args.apk.resolve()), "nativeLibraries": libraries,
          "arm64Only": bool(libraries) and all(lib["arm64"] for lib in libraries),
          "allElf16Kb": bool(libraries) and all(lib["elf16Kb"] for lib in libraries),
          "zipAlignmentVerified": False, "device16KbVerified": False}
text = json.dumps(report, ensure_ascii=False, indent=2)
if args.output:
    args.output.write_text(text + "\n", encoding="utf-8")
print(f"Native libraries: {len(libraries)}; ARM64 only: {report['arm64Only']}; all ELF 16 KB: {report['allElf16Kb']}")
for library in libraries:
    if not library["arm64"] or not library["elf16Kb"]:
        print(f"Gate pending: {library['path']}")
raise SystemExit(0 if report["arm64Only"] and report["allElf16Kb"] else 1)
