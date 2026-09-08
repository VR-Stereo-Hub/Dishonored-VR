"""Slice the yaw bookkeeping and its self-test out of head_track.cpp verbatim."""
import io, sys
LF = chr(10); BS = chr(92); NL = BS + "n"
src, dst = sys.argv[1], sys.argv[2]
lines = io.open(src, encoding="utf-8", newline="").read().replace(chr(13) + LF, LF).split(LF)
def find(pfx):
    for i, l in enumerate(lines):
        if l.startswith(pfx): return i
    raise SystemExit("marker not found: " + pfx)
si, ei = find("struct YawBook"), find("// Current GObjects membership")
ti, tj = find("static bool YawCase"), find("static bool FindPlayerController")
while not lines[tj - 1].strip(): tj -= 1
hdr = ("#define _CRT_SECURE_NO_WARNINGS" + LF + "#include <stdio.h>" + LF +
       "#include <stdint.h>" + LF + "#include <string.h>" + LF +
       "// Sliced VERBATIM from head_track.cpp; only Log() is shimmed." + LF +
       "template<class... A> static void Log(const char* f, A... a)"
       "{ printf(f, a...); printf(" + chr(34) + NL + chr(34) + "); }" + LF * 2)
tail = (LF + "int main(){ bool ok = YawSelfTest(); printf(" + chr(34) + "HOST RESULT: %s" + NL +
        chr(34) + ", ok?" + chr(34) + "PASS" + chr(34) + ":" + chr(34) + "FAIL" + chr(34) +
        "); return ok?0:1; }" + LF)
io.open(dst, "w", encoding="utf-8", newline=LF).write(
    hdr + LF.join(lines[si:ei - 1]) + LF * 2 + LF.join(lines[ti:tj]) + tail)
print("sliced %d lines of bookkeeping + %d of test" % (ei - si, tj - ti))

# The second executable exercises the actual guards and pawn writer, not just math.
# The only replaced function reads the engine's 32-bit GObjects header; host tests
# supply an in-memory table instead. All membership and ownership logic is verbatim.
from pathlib import Path
source_path = Path(src)
state = (source_path.parents[2] / "mod/state/32_game_dishonored_head_track.inc").read_text()
state = state[state.index("static uint8_t* g_yawCtrl"):state.index("static uint8_t* g_pcObj")]
end_owner = next(i for i, l in enumerate(lines) if l.startswith("// ---- the self-test:"))
owner = LF.join(lines[si:end_owner])
a = owner.index("static bool YawReadObjects(")
b = owner.index("static bool YawSlotMatches(")
owner = owner[:a] + "static bool YawReadObjects(void*** t, uint32_t* n) { *t = objectTable; *n = 2048; return true; }\n\n" + owner[b:]
(Path(dst).parent / "yaw_owner_impl.inc").write_text(state + LF + owner, encoding="utf-8")
