# Doc navigation: what to read, and how much of it

This repo's own docs are ~66,000 tokens - small enough to read IN FULL at the start of a
session, and that is what to do. The local reference mirrors under `docs/reference/` and
`docs/bsvr-reference/`, and the 2,989-class decompiled corpus under `tools/uscript/`, are
a different order of magnitude and must be grepped. That distinction is this file's whole
point: the budget is not scarce, but two directories in here will eat it in one command.

**This file is information, not instruction.** It says what is in which file and how each
is organised, so an answer can be found without opening the file that contains it - and so
that reading everything is a choice made knowingly rather than a default.

---

## The budget, measured

A session start of **50-80k tokens is the target**, chosen deliberately: accuracy at the
start is worth far more than the tokens, because a wrong assumption costs a whole session
and a headset run. Measured 2026-09-03 (bytes / 4):

| Read | ~Tokens | When |
|---|---:|---|
| `CLAUDE.md` | 4,300 | auto-loads, always |
| `docs/STATUS.md` | 16,500 | **always** |
| `docs/dishonored/ENGINE_NOTES.md` | 14,800 | **always** |
| `docs/ARCHITECTURE.md` | 6,700 | **always** |
| `docs/dishonored/HANDOFF-GINGASVR.md` | 6,500 | **always** - it is the traps and dead ends |
| `docs/VERIFICATION.md` | 3,900 | **always** |
| `docs/RESEARCH.md` | 2,400 | always |
| `docs/ROADMAP.md` | 2,000 | **always** |
| `docs/NAVIGATION.md` | 1,300 | this file |
| CODE_REVIEW, RELEASE_NOTES, TESTING, XR_HANDOFF, KNOWN_ISSUES, TROUBLESHOOTING | 7,700 | when relevant |
| **every project doc** | **~66,000** | affordable in one go |
| the five contract headers (below) | 5,400 | when touching that seam |
| the module in play (e.g. `aer.cpp` + `camera.cpp`) | 7,700 | yes, read the actual code |

**So read all the project docs.** They are ~66k tokens and they are the whole institutional
memory of this mod: nine sessions of measurements, the original author's traps, and the
decision log. Skimming them to save 40k tokens is a false economy - session 6 lost a run to
not checking which build was installed, which STATUS would have answered.

## The one rule

The rule is NOT "read less". It is **read the project's own docs generously, and never
bulk-read the two unbounded things**:

| | Size | How |
|---|---:|---|
| This repo's `docs/` | ~66k tokens | **read whole** |
| `docs/reference/`, `docs/bsvr-reference/` (BRVR, trilogy, Mirror's Edge mirrors) | ~40,000 LINES, 200k+ tokens | **grep only** |
| `tools/uscript/` (2,989 decompiled classes) | 5.3 MB, ~1.3M tokens | **grep only** |

Those last two are what actually blow a context window, and they are the ones a session
reaches for casually. One grep, one window, and say which file the answer came from.

## Starting a session

```
cat docs/STATUS.md docs/ROADMAP.md docs/ARCHITECTURE.md
cat docs/dishonored/ENGINE_NOTES.md docs/dishonored/HANDOFF-GINGASVR.md
cat docs/VERIFICATION.md docs/RESEARCH.md
git log --oneline -15
```

~55k tokens, and the session then knows what was measured, what was disproved, what the
instruments are and what the traps are. Add the contract headers and the module in play
when the work touches them:

```
cat src/core/gfx/stereo.h src/game/dishonored/camera.h src/core/framework/frame_hooks.h
```

**Then check what is actually installed** before believing any bug report is about your
tree - the log header carries the build tag:

```
tools\tail-log.ps1 -Grep "proxy loaded"      # or Select-String the log
git log --oneline -1
```

That check costs nothing and session 6 lost a whole run to skipping it.

## Routing by intent

| I need to know | Go to | How |
|---|---|---|
| What is the current state, what is verified | `STATUS.md` | the top block is live; below it is a dated archive |
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
