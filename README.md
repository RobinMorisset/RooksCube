# RooksCube

Exhaustive search for the maximum number of rooks that can be placed in an
`N × N × N` cube such that

1. **no rook is threatened by another rook** — no two rooks share an
   axis-parallel line (equivalently, no two rooks agree on two of their three
   coordinates), and
2. **no cell is threatened by three rooks** — for no cell of the cube do all
   three of its axis-parallel lines contain a rook (a rook does not threaten
   its own cell).

The search is a hand-written backtracker over the `N²` vertical *pillars*
`(x, y)`, each of which holds at most one rook. Instead of a boolean per cell,
the state is a set of bitfield **projections** onto the faces of the cube
(`proj_z_x`, `proj_x_y`, …) plus incrementally maintained `forbidden_*` masks,
so both constraints are enforced with a handful of bit operations and the inner
loop walks the still-allowed heights with `__builtin_ffs`. The recursion is
unrolled through templates (`Worker<on_initial_line, on_initial_column>`) so
the slice- and line-boundary tests are resolved at compile time. On top of that
sit several symmetry-breaking and bounding optimisations, each selectable at
compile time.

## Requirements

* A POSIX environment. `main.cpp` includes `<unistd.h>` and `<pthread.h>`
  unconditionally (they are only needed for `ROOKS_MONITOR`), so it does
  **not** compile against the MSVC headers; on Windows use WSL, MSYS2 or
  Cygwin.
* GCC or Clang — `__builtin_ffs` and `__attribute__((always_inline))` are
  required. C++11 or later.
* Verified with `g++ (Debian 8.3.0-6) 8.3.0` under WSL. Compiles clean apart
  from three `-Wunused-parameter` warnings.

## Building

One translation unit, no build system, and **everything is a compile-time
option** — `main()` ignores `argc`/`argv`. `N` has no default and must be
given, because the state is sized and the recursion unrolled at compile time.

The fastest configuration that is correct with the code as it stands today:

```bash
g++ -std=c++11 -O3 -march=native -DNDEBUG -DN=8 -DROOKS_PRINT=1 -DR_OPTIM2=1 -DR_OPTIM3=1 -DR_OPTIM6=1 -o rooks8 main.cpp -lpthread
```

Adding `-DR_OPTIM1=1 -DR_OPTIM4=1` makes it roughly 1.5× faster, but both are
broken as committed — see [Known issues](#known-issues).

Notes:

* **`-DROOKS_PRINT=1` is effectively mandatory.** `main()` never prints the
  final answer; the result is only visible as the stream of improving
  configurations that `ROOKS_PRINT` emits, so the last `----- BEST ! -----`
  block is the answer. In the printed grid, rows are `x`, columns are `y`, `*`
  is an empty pillar, and a digit or letter is the rook's height, 1-based
  (`A` = 10).
* `-lpthread` is only needed for `ROOKS_MONITOR`, but is harmless otherwise.
* Drop `-DNDEBUG` while experimenting: the `assert`s at the top of `backtrack`
  catch the index underflow that the `R_OPTIM1` bug below causes.
* Measured at `N=8`: `-O0` 2.4 s, `-O1` 1.11 s, `-Os` 1.09 s, `-O2` 0.98 s,
  `-O3` 0.94 s, `-O3 -march=native` 0.81 s. Use `-O3 -march=native`.

## Compile-time options

Every option is off unless defined to a true value. All of them are plain
`#if`, so `-DR_OPTIM3=0` also disables it.

### Symmetry breaking and bounding

| Macro | Effect | Depends on |
| --- | --- | --- |
| `R_OPTIM1` | Breaks the symmetry under permutation of the `x`-slices: requires the per-slice rook counts `cardinal_x[]` to be sorted. Also skips straight to the end of the current line once the count is too high. **Broken as committed.** | — |
| `R_OPTIM2` | Breaks the symmetry under permutation of the `y`-slices: requires the column projections `proj_x_y[]` to be sorted. Uses the `proj_x_y[N] = ~0` sentinel to avoid a special case at `j = N-1`. | — |
| `R_OPTIM3` | Breaks the symmetry under permutation of the heights: a height may only be used once all lower ones are, tracked by `max_z`. Biggest single win at small `N` (`N=5`: > 60 s → 2.7 s). | — |
| `R_OPTIM4` | Branch and bound: prunes when `card` plus an upper bound on the slots still available cannot beat `best_card`. | **`R_OPTIM1`.** The bound is `cardinal_x[i+1] * i + j`, which is only an upper bound if the slices still to come are known not to be larger than slice `i+1` — that is, under `R_OPTIM1`'s sorted invariant. Without it the prune happens to reproduce the known values up to `N=8`, but it is not justified. |
| `R_OPTIM6` | Tie-break for `R_OPTIM1`: when two adjacent slices have equal cardinality, orders them by `proj_y_x[]`. Inert on its own. | `R_OPTIM1`, to do anything |
| `R_OPTIM5` | **Experimental.** Prunes any branch in which a slice's cardinality falls more than 1 below the first slice's. Rests on a lemma that was never proved (commit 25c3f8e). It reproduces the known values up to `N=8` and halves the runtime there, but results obtained with it are not proofs. | — |

### Search bounds (for splitting work, or re-verifying a known value)

| Macro | Effect |
| --- | --- |
| `R_MIN=k` | Start from `best_card = k - 1`, i.e. only look for configurations with at least `k` rooks. Makes `R_OPTIM4` far more effective. If nothing is printed, no configuration with `k` or more rooks exists. |
| `R_MAX=k` | Prune branches projected to exceed `k` rooks, i.e. cap the search from above. Together with `R_MIN` this carves the search space into bands that could be run in parallel. |

Measured at `N=8`, whose optimum is 32: plain 0.96 s; `-DR_MIN=32` finds 32 in
0.05 s; `-DR_MIN=33` terminates in 0.03 s printing nothing, proving 32 optimal;
`-DR_MAX=31` returns 31.

### Instrumentation

| Macro | Effect |
| --- | --- |
| `ROOKS_PRINT` | Print every new best configuration. See the note above for the format. |
| `ROOKS_MONITOR` | Spawn a thread that prints the live configuration once a second. Needs `-lpthread`. |
| `R_COUNTER` | Print, at exit, how many branches each optimisation cut. Caveats: slot 0 counts *completed* configurations, not a prune; slots 1 and 2 always read 0, because the `R_OPTIM1`/`R_OPTIM2` conditions are caught earlier by the inline `OPTIM1_CHECK`/`OPTIM2_CHECK` inside `backtrack`, which do not increment; slot 3 is never written. |

## Reference values

Maximum number of rooks. Times are one core of a Ryzen-class laptop under WSL,
`-O2 -DNDEBUG`, with `R_OPTIM1` (repaired), `2`, `3`, `4` and `6`:

| `N` | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| max rooks | 1 | 2 | 6 | 9 | 12 | 18 | 22 | 32 | 36 |
| time | — | 0.00 s | 0.00 s | 0.00 s | 0.00 s | 0.00 s | 0.04 s | 0.95 s | 7 m 28 s |

`N = 1 … 4` were cross-checked against an independent brute force written from
the problem statement, which also gives `1, 2, 6, 9`. `N = 1` is not produced
by this program (see below). `N = 2 … 8` agree across every
correctness-preserving combination of the options that was tried, including
`R_OPTIM3` alone, which depends on nothing else.

## Known issues

1. **`R_OPTIM1` crashes for every `N ≥ 2`** — assertion `j < N`, or an
   out-of-bounds access under `-DNDEBUG`. A regression from commit b1d4947
   (*"Adapt the skip to also apply to optim1"*); its parent 16a46df is fine.
   The skip-ahead added there sets `j = 0` and then dispatches through
   `Worker_next<on_initial_line, on_initial_column, true>`, but
   `on_initial_column` was instantiated for the *original* `j`. When it is
   `true` (`j` was `N-1`), `Worker_next<false, true, true>::backtrack_next`
   does `backtrack(c, i, j - 1)`, and `j - 1` underflows `size_t`. Calling
   `recurse(c, i, j)` there instead — it already handles both the `j == 0` and
   the `j > 0` case — fixes it and reproduces every reference value above.
2. **Out-of-bounds read in `OPTIM1_CHECK`, and it is not benign.** The inline
   check at `main.cpp:224` runs for every template instantiation, including
   `on_initial_line`, where `i == N-1` and so `c->cardinal_x[i+1]` reads
   `cardinal_x[N]` — one past the end of the array, aliasing the `max_z`
   field. With `R_OPTIM3` on, `max_z` grows and the condition never fires, so
   the bug stays hidden; with `R_OPTIM3` off, `max_z` stays 1 and the first
   slice is silently capped at one rook. `R_OPTIM1 + R_OPTIM4` without
   `R_OPTIM3` therefore returns `N` instead of the true optimum (3, 4, 5, 6
   for `N = 3 … 6`). A `cardinal_x[N] = N` sentinel, mirroring the existing
   `proj_x_y[N]` one, removes both the undefined behaviour and the wrong
   answers.
3. **`N = 1` fails the assertion `i < N`.** `recurse_initial_line` lacks the
   `i == 0` termination check that `recurse` has, so the initial line of a
   single-slice cube recurses to `i = -1`. The answer is 1.
4. **`N ≤ 31`.** The projections are `uint32_t` and the code does `1 << i` on
   `int`, so `N = 32` would overflow. Runtime is the real limit long before
   that. `N_BITFIELD2` is declared but unused.
5. **The combination of symmetry breakers is not formally justified here.**
   `R_OPTIM1` (sort `x`-slices by cardinality), `R_OPTIM2` (sort `y`-slices by
   projection) and `R_OPTIM3` (heights in order) act on independent factors of
   the symmetry group and do compose: a `y`-permutation leaves `cardinal_x[]`
   untouched, and a `z`-permutation leaves both alone. `R_OPTIM6`, however,
   breaks ties in the `x`-ordering using `proj_y_x[]`, which a
   `y`-permutation does change, so its interaction with `R_OPTIM2` deserves a
   proof rather than the empirical agreement up to `N = 8` reported above.
