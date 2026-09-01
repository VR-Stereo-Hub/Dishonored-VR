"""Extract the ORIGINAL proxy's default dishonored_vr.ini as a golden file.

Build 38.92 wrote its default ini from one fprintf literal inside WriteDefaultIni
(src/dllmain.cpp at commit 48766c07). The table-driven config module that
replaces it must reproduce that file byte for byte (after the em-dash sweep),
so this script pulls the literal straight out of git and unescapes it.

Usage (from the repo root):
    python tools/ini-golden.py                # writes tests/golden/dishonored_vr.ini
    python tools/ini-golden.py --check FILE   # diff FILE against the golden
"""
import io
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden", "dishonored_vr.ini")
ORIGIN = "48766c07"  # the last single-file commit (proxy build 38.92)


def extract():
    src = subprocess.run(["git", "show", f"{ORIGIN}:src/dllmain.cpp"], cwd=ROOT,
                         capture_output=True, check=True).stdout.decode("utf-8")
    m = re.search(r"static void WriteDefaultIni\(const char\* ini\)\n\{.*?fprintf\(f,\n(.*?)\n\s*\"FlipRoll=1\\n\", kConfigVersion\);",
                  src, re.S)
    if not m:
        sys.exit("WriteDefaultIni literal not found")
    ver = re.search(r"static const int kConfigVersion = (\d+);", src).group(1)
    body = m.group(1) + '\n        "FlipRoll=1\\n"'
    text = ""
    for line in body.splitlines():
        line = line.strip()
        if not line.startswith('"'):
            continue
        lit = line[1:line.rfind('"')]
        # only the C escapes the literal actually uses; keeps the UTF-8 text intact
        text += re.sub(r'\\(.)', lambda e: {"n": "\n", "t": "\t", '"': '"', "\\": "\\"}.get(e.group(1), e.group(1)), lit)
    text = text.replace("%d", ver)
    return text


def main():
    text = extract()
    if len(sys.argv) >= 3 and sys.argv[1] == "--check":
        got = io.open(sys.argv[2], encoding="utf-8").read().replace("\r\n", "\n")
        want = io.open(GOLDEN, encoding="utf-8").read().replace("\r\n", "\n")
        norm = lambda t: t.replace("—", "-")
        if norm(got) == norm(want):
            print("ini golden: MATCH")
            return 0
        import difflib
        sys.stdout.writelines(difflib.unified_diff(norm(want).splitlines(True), norm(got).splitlines(True),
                                                   "golden", sys.argv[2]))
        return 1
    os.makedirs(os.path.dirname(GOLDEN), exist_ok=True)
    io.open(GOLDEN, "w", encoding="utf-8", newline="\n").write(text)
    print(f"wrote {GOLDEN}: {text.count(chr(10))} lines, {text.count('[')} sections")
    return 0


if __name__ == "__main__":
    sys.exit(main())
