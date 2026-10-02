"""Extract a checksummed SDK archive, dropping its single wrapper directory."""
import pathlib
import sys
import zipfile

archive, destination = map(pathlib.Path, sys.argv[1:])
destination.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(archive) as package:
    entries = [entry for entry in package.infolist() if not entry.is_dir()]
    roots = {pathlib.PurePosixPath(entry.filename).parts[0] for entry in entries}
    if len(roots) != 1:
        raise ValueError("SDK archive must contain one wrapper directory")
    for entry in entries:
        parts = pathlib.PurePosixPath(entry.filename).parts[1:]
        if not parts or any(part in ("..", ".") or ":" in part for part in parts):
            raise ValueError(f"Unsafe SDK archive entry: {entry.filename}")
        target = destination.joinpath(*parts)
        if not target.resolve().is_relative_to(destination.resolve()):
            raise ValueError(f"SDK entry escapes destination: {entry.filename}")
        target.parent.mkdir(parents=True, exist_ok=True)
        with package.open(entry) as source, target.open("wb") as output:
            import shutil
            shutil.copyfileobj(source, output)
if not (destination / "source.properties").is_file():
    raise ValueError("Extracted SDK package lacks source.properties")
print(f"Extracted {archive.name} into {destination}")
