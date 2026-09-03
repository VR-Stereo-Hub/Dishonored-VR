# Doc navigation: what to read, and how much of it

This repo's own docs are ~3,300 lines, and the local reference mirrors under
`docs/reference/` and `docs/bsvr-reference/` are another ~40,000. Read the wrong
way that is most of a context window before any work starts. Read the right way
a session start is under 4,000 tokens.

**This file is information, not instruction.** It says what is in which file and
how each is organised, so an answer can be found without opening the file that
contains it.

---

## The one rule

**Grep an anchor, read a window. Never open a doc whole above ~400 lines.**

Sizes, measured 2026-09-03:

| File | Lines | Read it whole? |
|---|---:|---|
| `docs/STATUS.md` | 921 | **No** - first ~120 lines only |
| `docs/dishonored/ENGINE_NOTES.md` | 892 | **No** - grep a heading |
| `docs/dishonored/HANDOFF-GINGASVR.md` | 540 | **No** - grep a section |
| `docs/ARCHITECTURE.md` | 375 | borderline; prefer a section |
| `docs/VERIFICATION.md` | 197 | yes, when you need the decision table |
| `docs/RESEARCH.md` | 133 | yes |
| `docs/ROADMAP.md` | 109 | yes |
| `docs/RELEASE_NOTES.md` | 84 | yes |
| `docs/dishonored/XR_HANDOFF.md` | 78 | yes |
| `docs/dishonored/TESTING.md` | 69 | yes |
| `docs/TROUBLESHOOTING.md` | 53 | yes |
| `docs/KNOWN_ISSUES.md` | 47 | yes |
| `docs/CODE_REVIEW.md` | 44 | yes |

`CLAUDE.md` (207 lines) auto-loads every session. That is the one fixed cost;
everything else is a choice.

---

## Starting a session

```
sed -n '1,120p' docs/STATUS.md      # the live handoff - current state, what is verified
git log --oneline -10
```

That is the whole start, ~2.5k tokens. **Do not read STATUS.md past line 120**
unless you want a specific past session: everything below the current-state
block is a dated session log going back to session 1, and nothing routes to it.
`grep -n "session 4" docs/STATUS.md` finds one when you need it.

Then read the ONE milestone in flight, not the ladder:

```
grep -n "^## S2a" -A 20 docs/ROADMAP.md
```

---

## Routing by intent

| I need to know | Go to | How |
|---|---|---|
| What is the current state, what is verified | `STATUS.md` | first 120 lines |
| What am I supposed to do next | `ROADMAP.md` | the one milestone |
| Why is the code shaped like this | `ARCHITECTURE.md` | the decision log is at the bottom, newest first |
| How do I prove a change worked | `VERIFICATION.md` | section 1 is a decision table: intent -> tool -> command -> how to read it |
| An engine address, offset, or a past measurement | `dishonored/ENGINE_NOTES.md` | `grep -n "^## "` for the heading list, then read that window |
| What the original author already tried and disproved | `dishonored/HANDOFF-GINGASVR.md` | section 11 is the process rules; the Traps and Dead ends sections are the ones that save a session |
| What a class or member is actually called | `tools/uscript/` | 2,989 decompiled classes, local only. `grep -rn --include=*.uc "name" tools/uscript/DishonoredGame/Classes` |
| How the BioShock mods solved the same problem | `docs/bsvr-reference/`, `docs/reference/` | local mirrors, gitignored. **Grep, never bulk read** |
| Prior art, legal posture, VR runtime facts | `RESEARCH.md` | whole file |
| A user-facing symptom | `TROUBLESHOOTING.md`, `KNOWN_ISSUES.md` | whole file |

---

## How each of the big three is organised

**`STATUS.md`** - newest first. A "Current state" block per session (the top one
is live), then "Session log" with a dated entry each. The current-state block is
the handoff; the log is the archive.

**`dishonored/ENGINE_NOTES.md`** - one `##` heading per finding, appended.
`grep -n "^## " docs/dishonored/ENGINE_NOTES.md` is the index. Every entry
carries the measurement that produced it and, where one exists, the log line to
look for.

**`ARCHITECTURE.md`** - the frame path and module design at the top, the
**decision log at the bottom**, newest first. A decision entry says what was
chosen, what was rejected, and the evidence.

---

## The reference mirrors

`docs/reference/` (the BioShock trilogy mod, BRVR, Mirror's Edge) and
`docs/bsvr-reference/` are **local only** and excluded. They are ~40,000 lines
and they will swamp a context window.

**Grep them for one subsystem, read the one window that matches, and say which
file the answer came from.** Never copy an offset or an address across - the
games are different binaries. Concepts and shapes port; numbers do not.

The live trees, if a mirror is stale: `C:\dev\Bioshock-Remastered-VR` and
`C:\dev\bioshock-trilogy-vr`. **Read only. Never build, commit or install from
them.**

---

## Cost notes

- `tools/build.ps1` output is thousands of lines. Filter it:
  `.\tools\build.ps1 2>&1 | Select-String -Pattern "error C|Build failed"`.
- `tools/lint.ps1` prints ~37 informational `note:` lines every run; only the
  last line is the verdict. `| Select-Object -Last 1`.
- The game log is 300+ lines a run. `tail-log.ps1 -Grep` or `Select-String` with
  a pattern, never `Get-Content` whole.
- `git diff` on a refactor can be thousands of lines. `--stat` first, then the
  one file.
