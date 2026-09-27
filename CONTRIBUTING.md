# Contributing to lvdExplorer

Thanks for considering a contribution. This project is young and the
architecture is still settling, so a quick heads-up before a large PR
saves everyone time — open an issue or discussion first for anything
beyond a small fix.

## Before you start

- Participation is covered by the [Code of Conduct](CODE_OF_CONDUCT.md).

## Development setup

See [README.md](README.md#building) for build instructions on Windows and
Linux. In short: Qt 6.5+, CMake 3.23+, and a C++20 compiler.

Run the test suite before opening a PR:

```
ctest --test-dir build --output-on-failure
```

## Code style

- Match the existing style in the file you're editing rather than
  introducing a new convention. This codebase generally follows the
  [Qt Coding Style](https://wiki.qt.io/Qt_Coding_Style) (braces on their
  own line for functions, `m_` prefix for member variables, etc.).
- Comments explain *why*, not *what* — if a comment just restates the
  code, it's the code that should be clearer, not the comment that's
  needed.
- No new external dependencies without discussion first — part of the
  point of this project is staying lightweight.

## Submitting a change

1. Fork the repo and create a branch for your change.
2. Keep the PR focused — one logical change per PR is much easier to
   review than a bundle of unrelated fixes.
3. Make sure `ctest` passes and the app still builds on whichever
   platform(s) you can test.
4. Open the PR with a description of *why*, not just *what* — the diff
   already shows what changed.

## Reporting bugs / requesting features

Use the issue templates — they ask for the specific details (platform,
Qt version, repro steps for bugs) that speed up triage.

## Questions

Open a [GitHub Discussion](../../discussions) (if enabled) or an issue
tagged as a question. There's no other support channel yet.
