"""Install an archived local candidate with complete INI/log evidence; never launch."""
import argparse
import configparser
import datetime
import difflib
import hashlib
import json
from pathlib import Path
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def crlf(data):
    rest = data.replace(b"\r\n", b"")
    if b"\n" in rest or b"\r" in rest:
        raise ValueError("INI is not consistently CRLF")


def settings(data):
    parsed = configparser.ConfigParser(strict=False, interpolation=None)
    parsed.read_string(data.decode("utf-8-sig"))
    return {(section, key): value for section in parsed.sections()
            for key, value in parsed.items(section)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--game-dir", type=Path, required=True)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    source, game = args.candidate.resolve(), args.game_dir.resolve()
    source.relative_to(repo / "build" / "playtest-candidates")
    if not (game / "Dishonored.exe").is_file():
        raise ValueError("Explicit destination does not contain Dishonored.exe")
    manifest = json.loads((source / "manifest.json").read_text())
    for name, key in [("d3d9.dll", "dllSHA256"), ("dishonored_vr.ini", "iniSHA256")]:
        if digest(source / name) != manifest[key]:
            raise ValueError("Candidate hash mismatch: " + name)
    if manifest["build"].encode() not in (source / "d3d9.dll").read_bytes():
        raise ValueError("Candidate DLL does not contain its build banner")
    before = (game / "dishonored_vr.ini").read_bytes()
    after = (source / "dishonored_vr.ini").read_bytes()
    crlf(before)
    crlf(after)
    old, new = settings(before), settings(after)
    changes = [[*key, old.get(key), new.get(key)]
               for key in sorted(old.keys() | new.keys()) if old.get(key) != new.get(key)]
    if args.verify_only:
        print(json.dumps({"verified": manifest["build"], "iniChanges": changes}))
        return
    archive = repo / "build/playtest-candidates/installs" / datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    archive.mkdir(parents=True)
    for name in ["d3d9.dll", "dishonored_vr.ini", "dishonored_vr.log", "dishonored_vr.prev.log"]:
        if (game / name).exists():
            shutil.copy2(game / name, archive / name)
    current = repo / "build/playtest-candidates/installed.json"
    if current.exists():
        shutil.copy2(current, archive / "previous-install.json")
    (archive / "ini-full.diff").write_text("".join(difflib.unified_diff(
        before.decode().splitlines(True), after.decode().splitlines(True),
        fromfile="previous", tofile="installed")))
    (archive / "ini-settings-diff.json").write_text(json.dumps(changes, indent=2))
    # All validation and backups precede the first install write.
    shutil.copy2(source / "d3d9.dll", game / "d3d9.dll")
    shutil.copy2(source / "dishonored_vr.ini", game / "dishonored_vr.ini")
    for name, key in [("d3d9.dll", "dllSHA256"), ("dishonored_vr.ini", "iniSHA256")]:
        if digest(game / name) != manifest[key]:
            raise ValueError("Installed hash mismatch: " + name)
    record = dict(manifest, archive=str(archive), candidate=str(source),
                  iniChanges=changes, gameLaunched=False)
    (archive / "installed.json").write_text(json.dumps(record, indent=2))
    current.write_text(json.dumps(record, indent=2))
    print(json.dumps({"installed": manifest["build"], "iniChanges": changes,
                      "archive": str(archive), "gameLaunched": False}, indent=2))


if __name__ == "__main__":
    main()
