# Codex session guide

Read [CLAUDE.md](CLAUDE.md) in full at the start of every session before investigating,
editing, building, or reviewing. It remains the shared project guide for all agents.
Read its required references for the task, including the latest flicker reference for
stereo instability, mono transitions, and weapon settling. Current user instructions
supersede older repository guidance.

## Session requirements

- The tester launches the game and reports observations. Run commands, build, install,
  configure diagnostics, and inspect logs yourself. Never launch the game.
- Check the installed Win32/dishonored_vr.log banner against the installed build before
  interpreting a playtest. Archive it and dishonored_vr.prev.log before another launch.
- Give one clearly defined question per launch, with expected results and what each
  outcome means. Do not ask the tester to execute commands.
- Build and install required candidates without requesting routine permission. Arm
  diagnostics in the installed ini. Compare the entire installed ini with its previous
  version on every install; preserve and verify CRLF with byte-aware edits.
- Engine-memory writers require IsLiveObject against a table current for the loaded
  level. A class-name check is not liveness. Revalidate retained identity after menus;
  an unchanged pointer does not establish that it is the same live object.
- Create or verify the Linear ticket before putting its number in code or a commit.
- Commits, PRs, and merges have no trailers or generated-by attribution. Branch names
  contain no personal names; use codex/vr-<ticket>-<description> for Codex work.
- Merge to VR-Main only with the user's explicit instruction to merge that work.
