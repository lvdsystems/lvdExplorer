# Performance

A replicable benchmark suite for tracking lvdExplorer's own performance release over release, modeled on the category breakdown [RoyiFileManager publishes](https://github.com/RoyiAvital/RoyiFileManager) (Pane Load, Filter Bar, Fuzzy Find, Selections, Navigation, Refresh/Selection).

**This is not a head-to-head comparison with RoyiFileManager or any other file manager.** Different app, different machine, different disk — none of that is controlled for. The point is to have lvdExplorer's own numbers, measured the same way every time, so a future release can be checked against this one.

## Running it

```
cmake --build build --target perftest --config Release
build/Release/perftest.exe
```

Always benchmark a **Release** build — Debug's timings don't reflect real usage and aren't comparable release over release. The suite isn't wired into `ctest`: it's a timing report, not a pass/fail check, and a full run takes noticeably longer than the rest of the test suite combined.

It builds its own fixtures on first run (in a `QTemporaryDir`, cleaned up after): a 1,000-file flat folder ("Small"), a 50,000-file flat folder ("Large"), and a 50-folder × 100-file nested tree (for the recursive search case). Each benchmark function uses Qt's `QBENCHMARK` macro, which adapts its own iteration count to hit a stable measurement — the `(total, iterations)` pair in the raw output shows how many times it actually ran.

## Methodology notes

- **Pane Load**: constructs a fresh `FileSystemModel`, points it at the folder, and waits for the scan to finish. This is the model-level cost; it doesn't include Qt's own view painting.
- **Filter Bar**: toggles the instant filter on and off against an already-loaded model (`FileSystemModel::setFilterText()` is a no-op when called with the text it already has, so the benchmark alternates between two strings to make sure every iteration does real work).
- **Filename Search**: one `SearchEngine` run (the Finder panel's engine) against the large folder (non-recursive) and the nested tree (recursive).
- **Selections**: `QItemSelectionModel::select()` over the whole model as one contiguous range.
- **Selection Readback**: `QItemSelectionModel::selectedRows()` after everything is already selected.
- **Navigation**: alternates between two distinct 1,000-file folders — approximates "switch to a sibling folder," since lvdExplorer always rescans on navigate rather than caching.
- **Refresh/Selection**: selects 100 rows, calls `FileSystemModel::refresh()`, then re-finds and reselects those rows by path in the repopulated model. This mirrors `FilePane::rememberSelection()`/`restoreSelection()` (`ui/pane/filepane.cpp`) — the mechanism that keeps a selection alive across an auto-refresh triggered by an external filesystem change.

## Results

Machine: AMD Ryzen Threadripper PRO 9965WX (24-core), 127 GB RAM, NVMe SSD, Windows 11 Pro. Numbers from one machine are only meaningful compared against other runs on that *same* machine — add a new column here when benchmarking a new version, on the same hardware, rather than comparing across machines.

| Test | 0.4.0 |
|---|---|
| Pane Load - Small Folder (1,000 files) | 5.3 ms |
| Pane Load - Large Folder (50,000 files) | 182 ms |
| Filter Bar - Small Folder | 0.019 ms |
| Filter Bar - Large Folder | 1.0 ms |
| Filename Search - Large Folder | 38 ms |
| Filename Search - Recursive (50×100 tree) | 6.0 ms |
| Selections - Small Folder | 0.0003 ms |
| Selections - Large Folder | 0.0003 ms |
| Selection Readback (50,000 selected) | 6.5 ms |
| Navigation (1,000-file folder ↔ 1,000-file folder) | 4.4 ms |
| Refresh/Selection (100 of 50,000 kept) | 199 ms |

Raw QTest output (iteration counts, totals): see `benchmark-results.txt` at the repo root, or regenerate with the command above.

### Notes on these numbers

- **Selections costs the same at 1,000 and 50,000 files (~0.0003 ms either way).** `QItemSelectionModel` stores a selection as a list of ranges, not per-row state, so selecting one contiguous range is O(1) regardless of row count. This isn't a measurement error — it's a real property of the current selection mechanism, and it's worth knowing before assuming selection work needs optimizing.
- **Refresh/Selection (199 ms) is dominated by the rescan itself**, not the reselect pass — compare against Pane Load - Large Folder (182 ms) for the same 50,000-file folder. The O(rowCount) path-matching loop that restores the selection adds relatively little on top.
- **Filename Search - Large Folder (38 ms) is slower than Filename Search - Recursive (6.0 ms)** despite searching far fewer total files in the recursive case (5,000 vs 50,000) — expected, since it's the same substring match over an order of magnitude more entries.
