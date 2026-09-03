---
name: commit
description: Stage and commit pending changes in this repo using the project's commit-message style. Use when the user wants to commit work in progress. Pass an optional one-line hint as argument to steer the message; otherwise infer from the diff.
allowed-tools: Bash(git status*), Bash(git diff*), Bash(git log*), Bash(git add*), Bash(git commit*), Bash(git restore --staged*)
---

## Commit pending changes

### Project commit-message style

Look at recent history before drafting — the canonical pattern is:

```
<Verb> <Subject>[: <details>]
```

Examples:
- `Refactor PageLesson: extract LessonButton and LessonIconButton inline components`
- `Add Android logcat message handler and link liblog`
- `Group the libs by kind: cpp/, qml/, tools/`

Conventions:
- English, capitalized first word, no trailing period
- Verbs in imperative: `Add`, `Update`, `Refactor`, `Fix`, `Remove`, `Rename`
- Optional `: <details>` clause for the "what specifically"
- **No** Conventional Commits prefixes (`feat:`, `fix:`) — the project does not use them
- **No** attribution or `Co-Authored-By` trailer
- One line is the norm; only add a body when the change genuinely needs explanation

### Steps

1. Run in parallel: `git status`, `git diff` (unstaged), `git diff --cached` (staged), `git log -5 --oneline` (style refresher).
2. If nothing to commit (no staged + no untracked + no modified) — tell the user and stop.
3. Review the diff:
   - Watch for accidental inclusion of the app database, `*.keystore`, `*.AppImage`, APKs, build outputs, `CMakeLists.txt.user` — all gitignored, but warn if any slipped through.
   - Watch for committed credentials or absolute paths.
4. Stage files explicitly by name (never `git add -A` / `git add .`), and prefer `git commit -- <paths>` so a parallel agent's staged work is not swept in.
5. Draft the commit message using the style above. If `$ARGUMENTS` is set, use it as steering input — but still rewrite into the project's style.
6. Show the user the proposed message + the file list, ask for confirmation before running `git commit`.
7. After commit: run `git status` to confirm a clean tree (or note remaining changes).

### Submodule handling

`libs` is a submodule. When it has its own changes, commit inside `libs` first, then commit the app together with the new submodule pointer — a pointer bump without the matching libs commit leaves the tree unbuildable for everyone else. If a pointer change appears that the user did not intend, surface it instead of committing it.

### Don't

- Do not commit the app database, keystore files, or anything matching `.gitignore`.
- Do not push. Committing only.
- Do not amend an existing commit unless the user explicitly asks.
- Do not use `--no-verify` to skip hooks.
- Do not add an attribution trailer.
