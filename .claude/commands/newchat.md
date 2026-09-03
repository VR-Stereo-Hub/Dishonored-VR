---
description: Close out a Dishonored VR session - bring STATUS, ROADMAP, ARCHITECTURE and ENGINE_NOTES up to date, check the repo and the installed build, then print a short prompt to paste into a fresh chat. Use when the user says they are stopping, wrapping up, ending the session, or wants a handoff written.
---

Close out this session so the next one starts from the real state instead of
re-deriving it, then hand the user a prompt to paste.

The point of this repo's doc system is that a new chat needs almost nothing said
to it: `CLAUDE.md` auto-loads, `docs/NAVIGATION.md` routes, and the top of
`docs/STATUS.md` holds the live handoff. This command's job is to make that
*true* before the session ends.

**Do not re-read the whole doc set to do this.** Follow `docs/NAVIGATION.md`:
grep an anchor, edit a window. Rewriting the top block of `STATUS.md` needs the
top block, not the file.

## 1. Update the handoff

**`docs/STATUS.md`** - rewrite the top "Current state" block for this session,
and append a dated entry to the Session log. The current-state block is what the
next session reads; the log is the archive. It must carry:

- **What landed**, with the *evidence* that proved it. "Fixed" with no
  measurement is how handoffs rot. Quote the log line.
- **What is verified versus assumed**, said plainly. Build-verified is not
  headset-verified and must never be written as if it were.
- **What is installed in the game folder right now**, and whether it matches the
  current build. A session that leaves those disagreeing has to say so.
- **The next step** - one or two sentences naming the single most valuable thing
  to do next, and why it is that rather than something else.
- **Any dead end**, with the line that killed it. "This approach is dead and
  here is why" is worth as much as a fix.

**`docs/ROADMAP.md`** - tick what completed. Only tick a box when its "done
when" actually holds; a box ticked on code landing rather than on a measured
effect is a lie the next session will believe.

## 2. Check the other docs did not drift

- New engine address, offset or UE3 field -> `docs/dishonored/ENGINE_NOTES.md`,
  **with its derivation** and the log line that shows it. `patterns.h` carries
  the number, ENGINE_NOTES carries the why.
- A non-obvious design choice, or an approach rejected -> the decision log at
  the bottom of `docs/ARCHITECTURE.md`, dated.
- A new instrument, or one that turned out to be stale -> `docs/VERIFICATION.md`
  section 1. An instrument that lies is worse than none.
- A user-facing symptom -> `docs/KNOWN_ISSUES.md` / `docs/TROUBLESHOOTING.md`
  (these ship in the release zip).
- Doc sizes moved a lot -> refresh the table in `docs/NAVIGATION.md`.
- Any comment this session made false -> fix it now.

## 3. Check the repo

- `git status --short` - nothing uncommitted that should be committed.
- Commit messages explain **why**, not just what. Conventional prefixes, no
  trailers, no AI attribution, no em dashes.
- `tools\lint.ps1 | Select-Object -Last 1` reads `lint: clean`.
- Say whether the installed `d3d9.dll` matches the current build - compare the
  build tag in the log header against `git log --oneline -1`. **Read the version
  stamp; do not assume the user is running your build.** That mistake cost a
  whole session once.
- Ask before pushing.

## 4. Print the prompt

Output a block for the user to paste into a fresh chat. **Three lines or fewer.**
If it needs more, `STATUS.md` is not doing its job and should be fixed instead of
compensated for.

```
Read docs/NAVIGATION.md, then the first 120 lines of docs/STATUS.md, and pick up
where we left off.
Current focus: <one line naming the next step>
```

Add one extra line only for something the files genuinely cannot carry: a
machine change, an uncommitted edit, a run that must happen before anything else.

Then tell the user, one sentence each:

- what landed this session,
- what is verified versus assumed,
- what the next session starts on.

Be honest about anything untested in a headset. Never imply something works when
it has only been built.
