"""Extract the proxy's default dishonored_vr.ini as a golden file.

WriteDefaultIni (src/core/config/config.cpp) writes the default ini from one
fprintf literal. This script unescapes that literal from the WORKING TREE so
tests/golden/dishonored_vr.ini is the file a fresh install gets; a change to
the literal without a regenerated golden fails --check. (Until 41.0 the golden
was the 38.92 literal pulled from commit 48766c07; the removals of 41.0 made
that comparison meaningless, see docs/RELEASE_NOTES.md.)

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
SOURCE = os.path.join(ROOT, "src", "core", "config", "config.cpp")
VERSRC = os.path.join(ROOT, "src", "mod", "state", "15_core_config_config.inc")


def extract():
    src = io.open(SOURCE, encoding="utf-8").read()
    m = re.search(r"static void WriteDefaultIni\(const char\* ini\)\n\{.*?fprintf\(f,\n(.*?)\n\s*\"FlipRoll=1\\n\", kConfigVersion\);",
                  src, re.S)
    if not m:
        sys.exit("WriteDefaultIni literal not found")
    ver = re.search(r"static const int kConfigVersion = (\d+);",
                    io.open(VERSRC, encoding="utf-8").read()).group(1)
    body = m.group(1) + '\n        "FlipRoll=1\\n"'
    text = ""
    for line in body.splitlines():
        line = line.strip()
        if not line.startswith('"'):
            continue
        lit = line[1:line.rfind('"')]
        # only the C escapes the literal actually uses; keeps the UTF-8 text intact
        text += re.sub(r'\\(.)', lambda e: {"n": "\n", "t": "\t", '"': '"', "\\": "\\"}.get(e.group(1), e.group(1)), lit)
    # Spend the fprintf conversions the way fprintf does, in ONE left-to-right
    # pass. Doing it as two replaces gets "%%d" wrong, and skipping "%%"
    # entirely (which this did until 2026-09-12) leaves "%%LOCALAPPDATA%%" in
    # the golden where the runtime writes "%LOCALAPPDATA%" - a difference the
    # broken --check below could never report.
    out, i = [], 0
    while i < len(text):
        if text[i] == "%" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == "%":
                out.append("%"); i += 2; continue
            if nxt == "d":
                out.append(ver); i += 2; continue
        out.append(text[i]); i += 1
    return "".join(out)


def main():
    text = extract()
    if len(sys.argv) >= 3 and sys.argv[1] == "--check":
        got = io.open(sys.argv[2], encoding="utf-8").read().replace("\r\n", "\n")
        # Compare against the SOURCE literal, not against the golden file. This
        # read GOLDEN until 2026-09-12, so `--check tests/golden/...` compared
        # the golden to itself and could only ever print MATCH - including for
        # the one thing it exists to catch, a literal edited without a
        # regenerated golden. It never failed because it never could.
        want = text.replace("\r\n", "\n")
        norm = lambda t: t.replace(chr(0x2014), "-")
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
