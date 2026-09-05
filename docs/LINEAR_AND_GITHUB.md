# Linear and GitHub

How a change gets from "someone noticed something" to "that shipped": the ticket, the branch,
the pull request, the review, the merge, and the release. It binds humans and agents to the
same flow, because the alternative is what this project had until 2026-09-05, when three
people were finding real faults and recording them in three private places.

The board lives at [linear.app/vr-stereo-hub](https://linear.app/vr-stereo-hub), team **VR**,
project **Dishonored VR Mod**. `CLAUDE.md` carries the short version of these rules; this file
is the long one.

## What goes on the board, and what does not

The board carries **work**: a thing to do, with an owner, a milestone and a definition of done.
It is not the project's memory.

The memory stays in `docs/`, and the split is not negotiable because the two rot at different
speeds. An engine address, a class layout, a falsified hypothesis, the reason a lever exists -
those go to `docs/dishonored/ENGINE_NOTES.md` or `docs/ARCHITECTURE.md` and stay true for
years. A ticket is true until it is closed. Putting a derivation in a ticket description buries
it the moment the ticket is done, and the next person re-derives it.

So: a ticket **links** to the notes and quotes the numbers it needs to be actionable. The notes
never cite a ticket as their source of record.

## The map

| Thing | Here |
|---|---|
| Team | **VR** - one team for every mod in the org |
| Project | **Dishonored VR Mod** - one project per repository |
| Milestone | **A version.** `41.1`, `41.2`, `42.0`, `42.1`. Kept in sync with the repo's GitHub Releases page |
| Cycles | **Not used.** Two developers working in evening sessions; milestones carry the schedule |
| Estimates | **Not used.** If a ticket is too big to judge at a glance, split it |

A milestone is a version because that is the only boundary this project actually has. There is
no sprint and no quarter; there is "the build a tester installs". `docs/ROADMAP.md` keeps the
engineering ladder (S0 to S3) and each milestone's description names the rungs it closes, so
the two views agree without either being a copy of the other.

## Statuses

Linear's categories are fixed (Backlog, Unstarted, Started, Completed, Canceled, Duplicate) and
every status belongs to exactly one. Ours:

| Status | Category | What it means here |
|---|---|---|
| **Backlog** | backlog | Accepted and real, not scheduled. **This is the inbox**: anyone files straight here |
| **Todo** | unstarted | Scheduled for the current milestone. The next thing someone picks up |
| **In Progress** | started | A branch exists. Linear sets this automatically when the PR opens |
| **In Review** | started | The PR is ready to read, with its simulator results and its headset run already in the body |
| **Done** | completed | Merged to `VR-Main` and its pass criteria measured. Not yet in anyone's hands |
| **Released** | completed | Shipped in a tagged build on the GitHub Releases page |
| **Canceled** | canceled | Not doing it. The comment says why |
| **Duplicate** | duplicate | System-managed by Linear |

Two states this project deliberately does **not** have, because both were considered and
rejected:

- **No Triage.** Backlog is the inbox. A third person joining does not justify a queue that
  someone has to drain.
- **No "needs headset test".** A PR must not merge without a headset run, so the state would
  never be occupied. The headset run happens **before** review ends and its result goes in the
  PR body. Where acceptance is purely perceptual, the ticket carries `needs-headset` and the
  work of judging it *is* the ticket.

`Done` and `Released` are separate because they answer different questions. `Done` means the
code is on `VR-Main`. `Released` means a person can install it. Between them sit the docs
reconciliation, the packaging and the tag, and that gap is where a project starts believing it
has shipped things it has not.

## Priority

Linear gives five levels and no more, on purpose. What earns each one here:

| Priority | Earns it |
|---|---|
| **Urgent** | A tester cannot play, or cannot trust what they are seeing. The deep-crouch climb and the ghosting were both this |
| **High** | The current milestone cannot close without it |
| **Medium** | A real fault with a workaround, or a feature the milestone wants but can ship without |
| **Low** | Polish, carried items, cleanups: anything whose absence nobody will notice |
| **None** | Not triaged yet. Should not survive a week |

Priority is about **the current milestone**, not about the project forever. A 42.1 parity item
is `Low` today and may be `High` when 42.1 is the milestone in flight. Re-prioritise when a
milestone opens; do not agonise over it before then.

## Labels

- **`Type` (a group, so exactly one per ticket)**: `Bug`, `Feature`, `Improvement`, `Docs`,
  `Chore`, `Research`, `Regression`.
  `Research` is the one worth explaining: its deliverable is a **measurement and a written
  verdict**, not code. It closes with the answer, and the answer opens whatever ticket it
  implies. A `Research` ticket that closes with "and we fixed it" was mislabelled.
- **`area:` labels (as many as apply)**: `render`, `camera`, `input`, `hands`, `hud`,
  `runtime`, `perf`, `tooling`, `engine`, `config`.
- **Flags**: `needs-headset` (acceptance is perceptual; the simulator cannot close it),
  `needs-simulator` (has a sequence or an eye-check leg that must pass first), `blocked`,
  `good-first-issue`, `gingas-parity` (a fix from the original author's 39.x line).

## The ticket template

A ticket has two readers and they want different things. A human wants to know what is wrong
and whether it matters. An agent needs enough to act without this conversation: the files, the
levers that already exist, what not to touch, and how anyone will know it worked.

```markdown
## Summary
<Two to four plain sentences. What a player or a maintainer would notice, and what changes.
 No jargon, no engine internals. If someone read only this, they should know whether they
 care.>

## Why now
<The milestone it serves and the evidence that it matters: a measurement, a log line, a
 headset verdict, a count. "It feels wrong" is a starting point, not a reason.>

## Technical detail
<The lane (present thread or script thread), the seam, the files, the existing levers to
 reuse, and what is already known. Link ENGINE_NOTES and STATUS rather than restating them.
 Name what has already been falsified so nobody re-walks it.>

## Blast radius
<What else this touches, what could regress, and what the fail-soft is. If a wrong fix here
 puts the player through a floor, say so.>

## Non-goals
<Explicitly out of scope, so the work does not widen. This is the most valuable section for
 an agent and the one most often left out.>

## Pass criteria
<Falsifiable, measured, in the order they should be run. Name the tool and the number.
 Simulator first, headset last.>
- [ ] simulator: <command> -> <expected>
- [ ] headset: <what a human must judge, and the A/B that would disprove it>

## Links
<ENGINE_NOTES sections, STATUS run numbers, the PR, related tickets.>
```

Small tickets may drop `Why now`, `Blast radius` and `Non-goals`. **No ticket ships without
`Summary` and `Pass criteria`.** A ticket with no pass criteria cannot be finished, only
abandoned.

### What makes pass criteria good here

This project's own rules, applied to acceptance:

- **A verified write is not an honoured one.** "The field now reads 0.32" is not acceptance.
  "The view pivots at the eyes" is.
- **An instrument that cannot fail its own hypothesis is not evidence.** If the check can only
  come out one way, it is not a check.
- **A counter is not evidence until you know its population.** Say what would make it move.
- **Simulator first, headset last.** Anything `tools\xrsim-*` can answer must be answered
  there. A headset run costs a person their evening.
- **One rig is one data point.** Where a default is being chosen, say so and ask for a second.

## The flow

### 1. Find the ticket, or write it

Search Linear before creating anything. Half of what gets "found" is already filed, sometimes
by the other developer that week.

If it does not exist, create it from the template with **project, milestone, priority and at
least a `Type` label filled in**. A ticket missing those is invisible on every view that
matters.

If the work is not in the current milestone and is not urgent, it still gets filed. Filing is
cheap; remembering is not.

### 2. Branch

```
<owner>/vr-<n>-<short-slug>
```

This is Linear's own "copy git branch name" format (`Ctrl + Shift + .` on the issue), so the
easiest way to get it right is to copy it from the ticket. `<owner>` is your Linear username,
or `claude` for an agent session. Branch off `VR-Main`.

The branch name alone is enough for Linear to link the PR, but the PR body says it too, because
branches get renamed and merged bodies do not.

### 3. Work

Nothing about the engineering rules changes. `CLAUDE.md` still governs: one behavioural change
per build, findings to ENGINE_NOTES in the same commit as the code, every render lever default
OFF with a live A/B toggle, no em dashes, no game-derived content, byte-verified hooks.

Commit messages stay plain conventional commits, imperative, subject 72 characters or less, no
trailers and no AI attribution. **Do not put `Fixes VR-<n>` in a commit message**: on GitHub a
magic word in a commit moves the issue when the commit reaches the default branch, which
double-fires against the PR and clutters the ticket. The PR body is the single link.

### 4. Open the pull request

**The first line of the PR body is the link:**

```
Fixes VR-42
```

- `Fixes VR-<n>` when the PR's base is `VR-Main`. Merging it closes the ticket.
- `Ref VR-<n>` when the PR's base is a working branch (a stacked PR). It links without closing,
  so only the PR that actually reaches `VR-Main` marks the ticket Done.
- Several tickets on one PR: `Fixes VR-42, VR-43`.
- To attach a PR to a ticket with no status effect at all, use `Ref`.

The PR title is a conventional-commit subject, the same shape as a commit: `feat:`, `fix:`,
`docs:`, `build:`, `tools:`, `chore:`, `refactor:`.

Then fill in `.github/PULL_REQUEST_TEMPLATE.md`. The contract it encodes:

- **What a player would notice**, before any mechanism.
- **The evidence**: numbers, before and after, log lines, run identifiers. A PR that changes
  behaviour and shows no measurement is not ready to read.
- **Every new lever, its default and its live A/B.** Default OFF unless there is a stated
  reason, and an exception is argued in the body rather than assumed.
- **Blast radius and fail-soft.**
- **What is deliberately not here**, with the reason. Faults found and not fixed get their own
  ticket, and the PR names it. This is how PR #14 and PR #15 handed over four real defects
  instead of losing them.
- **Testing**: which simulator sequences ran, which headset runs happened on which rig, and
  build, lint, exports and the golden ini.

### 5. Review

Opening the PR moves the ticket to **In Progress** automatically. Requesting review moves it to
**In Review**.

The reviewer's job is not to re-derive the change. It is to ask:

- Does the evidence support the claim? A parked commit's message is not evidence, and this
  project has already paid for taking one at face value.
- Was the build that produced the verdict actually installed? A log banner and a file hash
  settle it, and at least one session's first "it's fixed" was measured on a binary that had
  been compiled and never installed.
- Does every new lever have an A/B, and does every refused guard log why, with the values?
- Are the docs in the same PR as the code they describe?

Review comments go on the PR. **Measurements, verdicts and decisions go on the ticket**, because
the ticket outlives the branch and someone will look for them in six months.

### 6. Merge

Merge to `VR-Main`. Linear moves the ticket to **Done**. Delete the branch.

Then the session-end ritual from `CLAUDE.md`, unchanged: rewrite "Current state" and "Next
steps" in `docs/STATUS.md`, append a dated session log entry, tick `docs/ROADMAP.md` boxes, add
a dated entry to `docs/ARCHITECTURE.md`'s decision log if a non-obvious choice was made, push.

If a group of tickets closed together, post a **batch project update**. Not one per ticket.

## Project updates

Two kinds, both on the project in Linear. Health is one of three values: **On track**,
**At risk**, **Off track**. Post honestly; a permanently green project is one whose updates
nobody reads.

### The batch update

Posted when a group of work lands, or roughly weekly while work is active. It is what a person
reads to catch up without reading fifteen PRs.

```markdown
**Window**: <dates>, <n> sessions, PRs #<a> to #<b>.

## What landed
<Prose, not a changelog. What changed for a player, and what the mechanism was.
 Numbers where they exist.>

## What was measured
<The findings worth carrying: what turned out to be true, and what was falsified.
 A falsified hypothesis is a result and belongs here.>

## What is open
<Named, with the ticket. Faults found and deliberately not fixed go here, not nowhere.>

## Risks
<What the work rests on that might not hold. "Judged on one rig" is a risk.>

## Next
<The next milestone's shape, in a sentence or two.>
```

### The release update

Posted when a version ships. Separate from the batch update, because its audience includes
people who do not read the board.

```markdown
**Release <version>** - tag `<tag>`, <date>, <n> tickets.

## For players
<What is new and what is fixed, in plain language. No internal names.>

## Known issues
<What is still wrong, and the workaround. Points at KNOWN_ISSUES.md.>

## Upgrading
<Ini keys removed or renamed, settings that must change, anything that breaks a tuned setup.>

## What went in
<The ticket list, grouped by Type.>
```

## The release ritual

**Only a human declares a release. An agent never does, and never proposes a version number as
though the decision has been made.** An agent may report that a milestone's tickets are all Done
and ask whether to cut it. That is the whole of an agent's role here.

When the user names the version:

1. **Check the milestone.** Every ticket in it is `Done`. Anything that is not either moves to
   the next milestone or the release waits. Do not ship a milestone with open tickets in it and
   call it done.
2. **Reconcile the documents.** `docs/STATUS.md` "Current state" describes HEAD.
   `docs/RELEASE_NOTES.md` has exactly one section for this version and it stops saying
   `(unreleased)`. `docs/KNOWN_ISSUES.md` and `docs/ROADMAP.md` match the code.
3. **Build the artifact.** `.\tools\package.ps1`. It refuses on a `-dirty` tree, because a log
   from a dirty build cannot be traced to a commit.
4. **Tag and publish.** A git tag for the version, then a GitHub release on the Releases page
   with the zip attached. The release notes come from the milestone's tickets; the same content
   goes into `docs/RELEASE_NOTES.md`.
5. **Post the release update** on the Linear project.
6. **Move every ticket in the milestone from `Done` to `Released`.**
7. **Close the milestone and open the next one.**

Step 6 is the one that gets skipped and it is the one that makes `Released` worth having. A
ticket sitting in `Done` after its version shipped is a lie about what a player can install.

## Working with agents

Agents (Claude Code sessions, Linear's own agent) follow the same flow with three additions:

- **An agent never declares a release**, per above.
- **An agent creates the ticket if it is missing**, from the template, rather than doing
  untracked work. A change with no ticket is a change nobody else can see coming.
- **Delegation is not assignment.** In Linear, delegating an issue to an agent keeps the human
  as assignee and owner. The human stays responsible for the verdict.

Ticket descriptions are also agent prompts, so the sections that constrain matter most:
`Technical detail` naming the files and the levers that already exist, `Non-goals` saying what
not to touch, and `Pass criteria` giving something falsifiable to aim at. A ticket that says
"make the hands work" produces a session of exploration; one that names the SkelControl drive,
says the code is compiled but untested on this render, and asks for a picture at two yaws
produces a change.

## Never quote a chat verbatim

Adopted as a hard rule in PR #15, and it applies to everything published: commit messages, PR
bodies, Linear tickets and comments, code comments, `docs/`.

A perceptual report is evidence and belongs in the record. Its exact wording never is. Report
the observation and every fact survives, without putting someone's casual sentence in front of
strangers permanently. Numbers, log lines, ini keys, the game's own shipped comments and a
person's own written PR body stay quotable.

## What only the Linear UI can do

The MCP cannot reach any of these. If something below looks wrong, it is a settings change, not
a bug in a script.

| Setting | Where |
|---|---|
| Create or reorder workflow statuses | Settings > Team > Issue statuses and automations |
| PR automation rows and branch-specific rules | same page |
| Branch name format | Settings > Integrations > GitHub > Branch format |
| Issue templates, and setting a team default template | Settings > Team > Templates |
| "On git branch copy, move issue to started status" | Settings > Account > Code and reviews |
| Agent guidance, workspace and per-team | Settings > Agents > Additional guidance |

The automation rows this project wants:

| Row | Value |
|---|---|
| On pull request opened | In Progress |
| On review requested or activity | In Review |
| On PR or commit merge | Done, **restricted to base `VR-Main`** |
| Any other base branch | no action |

The restriction is what makes stacked PRs behave: a PR merged into a working branch must not
close its ticket, because the change has not reached `VR-Main` yet.

## Quick reference

```
1. Search Linear. Create from the template if it is not there.
   Project + milestone + priority + Type label, always.
2. Branch: <owner>/vr-<n>-<slug>, off VR-Main. Copy it from the ticket.
3. Work. Simulator first. Headset last. ENGINE_NOTES in the same commit.
4. PR body line 1: "Fixes VR-<n>" into VR-Main, "Ref VR-<n>" into a working branch.
   Title: a conventional-commit subject.
5. Fill the PR template. Evidence, levers and defaults, blast radius,
   what is deliberately not here, testing.
6. Review, then merge to VR-Main. Linear marks it Done.
7. STATUS, ROADMAP boxes, decision log, push. Batch project update if several closed.
8. Release only when the user says so. Then Done -> Released, milestone closed.
```

## Related documents

| File | Purpose |
|---|---|
| `CLAUDE.md` | The short version of these rules, plus every engineering rule |
| `docs/STATUS.md` | Session handoff: current state, next steps, blockers, session log |
| `docs/ROADMAP.md` | The engineering ladder S0 to S3 and the carried items |
| `docs/VERIFICATION.md` | Intent, to tool, to command, to how to read the result |
| `docs/RELEASE_NOTES.md` | Per-version notes; the release ritual writes here |
| `docs/KNOWN_ISSUES.md` | User-facing known issues; ships in the zip |
