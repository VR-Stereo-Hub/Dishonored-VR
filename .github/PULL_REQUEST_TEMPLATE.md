Fixes VR-

<!--
  Line 1 above is the Linear link and it must be the first line of the body.
    Fixes VR-42   when this PR targets VR-Main. Merging it closes the ticket.
    Ref   VR-42   when this PR targets a working branch. Links without closing.
  No ticket yet? Create one first. See docs/LINEAR_AND_GITHUB.md.

  Title: a conventional-commit subject - feat: / fix: / docs: / build: / tools: /
  chore: / refactor:, imperative, 72 characters or less.

  Never quote a chat verbatim anywhere in this body. Report the observation instead.
  Numbers, log lines, ini keys and the game's own comments stay quotable.
-->

## What this is

<!-- What a player or a maintainer would notice, before any mechanism. Two to four sentences. -->

## What changed

<!-- Per area or per commit. A table works well when there are more than three. -->

| Change | What |
|---|---|
|  |  |

## Evidence

<!--
  Numbers. Before and after. Log lines. Run identifiers and the rig.
  A PR that changes behaviour and shows no measurement is not ready to read.
  If a hypothesis was falsified on the way, say so: that is a result.
-->

## Levers and defaults

<!--
  Every new lever, its ini key, its seam word, its F10 control and its default.
  Default OFF unless there is a stated reason; argue any exception here.
  A seam word with no F10 control is not shipped.
-->

| Lever | Default | Live A/B |
|---|---|---|
|  |  |  |

## Blast radius and fail-soft

<!-- What else this touches, what could regress, what happens when it refuses. -->

## What is deliberately not here

<!--
  Faults found and not fixed, mechanisms ruled out, work split off on purpose.
  Each one gets its own ticket; name it here. This is how a PR hands over what it
  learned instead of losing it.
-->

## Testing

- [ ] Simulator: <!-- which tools/xrsim sequences, and their results -->
- [ ] Headset: <!-- rig, runtime, date, and the verdict in the tester's terms -->
- [ ] `.\tools\build.ps1` clean
- [ ] `.\tools\lint.ps1` clean (this is the em-dash gate)
- [ ] `.\tools\exports-check.ps1` 9 of 9 undecorated
- [ ] Golden ini matches, or the change to it is intentional and named above
- [ ] The installed build is the built build (log banner and file hash agree)

## Docs

- [ ] `docs/STATUS.md` current state and next steps
- [ ] `docs/ROADMAP.md` boxes ticked
- [ ] `docs/dishonored/ENGINE_NOTES.md` for any address, offset, layout or falsified mechanism
- [ ] `docs/ARCHITECTURE.md` decision log, if a non-obvious choice was made
- [ ] `docs/KNOWN_ISSUES.md` / `docs/RELEASE_NOTES.md`, if a player sees the difference
