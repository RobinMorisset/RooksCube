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

The recommended configuration:

```bash
g++ -std=c++11 -O3 -march=native -DNDEBUG -DN=8 -DROOKS_PRINT=1 -DR_OPTIM1=1 -DR_OPTIM2=1 -DR_OPTIM3=1 -DR_OPTIM4=1 -DR_OPTIM6=1 -o rooks8 main.cpp -lpthread
```

`R_OPTIM4` is the one you cannot do without — with everything else on but
`R_OPTIM4` off, `N = 7` does not finish in 200 s, against 0.03 s with it — and
its bound is only valid under `R_OPTIM1`, so enable those two together. The
build enforces that: `R_OPTIM4` or `R_OPTIM6` without `R_OPTIM1` is an
`#error`.

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
| `R_OPTIM1` | Breaks the symmetry under permutation of the `x`-slices: requires the per-slice rook counts `cardinal_x[]` to be sorted. | — |
| `R_OPTIM2` | Breaks the symmetry under permutation of the `y`-slices: requires the column projections `proj_x_y[]` to be sorted. Uses the `proj_x_y[N] = ~0` sentinel to avoid a special case at `j = N-1`. | — |
| `R_OPTIM3` | Breaks the symmetry under permutation of the heights: a height may only be used once all lower ones are, tracked by `max_z`. Biggest single win at small `N` (`N=5`: > 60 s → 2.7 s). | — |
| `R_OPTIM4` | Branch and bound: prunes when `card` plus an upper bound on the slots still available cannot beat `best_card`. | **`R_OPTIM1`.** The bound is `cardinal_x[i+1] * i + j`, which is only an upper bound if the slices still to come are known not to be larger than slice `i+1` — that is, under `R_OPTIM1`'s sorted invariant. Enabling it without `R_OPTIM1` is refused at compile time. |
| `R_OPTIM6` | Tie-break for `R_OPTIM1`: when two adjacent slices have equal cardinality, orders them by `proj_y_x[]`. | **`R_OPTIM1`**, whose order it refines; enabling it alone is refused at compile time. |
| `R_OPTIM5` | **Experimental.** Prunes any branch in which a slice's cardinality falls more than 1 below the first slice's. Rests on a lemma that was never proved (commit 25c3f8e). It reproduces the known values up to `N=8` and halves the runtime there, but results obtained with it are not proofs. | **`R_OPTIM1`** — it compares each slice against `cardinal_x[N-1]`, which is the largest slice only under `R_OPTIM1`'s ordering. Enforced at compile time. |

### Search bounds (for splitting work, or re-verifying a known value)

| Macro | Effect |
| --- | --- |
| `R_MIN=k` | Start from `best_card = k - 1`, i.e. only look for configurations with at least `k` rooks. Makes `R_OPTIM4` far more effective. If nothing is printed, no configuration with `k` or more rooks exists. **Requires `R_OPTIM4`**, enforced at compile time: `R_OPTIM4` is the only thing that consults `best_card` for pruning, so on its own `R_MIN` costs a full search and merely hides the configurations below `k`. Use `k ≥ 1`; `R_MIN=0` underflows `best_card` to `SIZE_MAX` and the run prints nothing at all. |
| `R_MAX=k` | Prune branches projected to exceed `k` rooks, i.e. cap the search from above. Together with `R_MIN` this carves the search space into bands that could be run in parallel. **Requires `R_OPTIM5`**, enforced at compile time: it prunes on `(cardinal_x[N-1]-1)*i + card`, which bounds from below what a subtree can still reach only if every slice ends within one rook of `cardinal_x[N-1]` — exactly what `R_OPTIM5` enforces. So `R_MAX` inherits `R_OPTIM5`'s unproved lemma, and a run that uses it is not a proof. |

Measured at `N=8`, whose optimum is 32, `-O3 -march=native` with `R_OPTIM1`,
`2`, `3`, `4`, `6`: plain 0.79 s; `-DR_MIN=32` finds 32 in 0.04 s; `-DR_MIN=33`
terminates in 0.02 s printing nothing, proving 32 optimal. Adding `R_OPTIM5`,
as `R_MAX` requires: `-DR_MAX=31` returns 31 in 0.52 s and `-DR_MAX=32`
returns 32 in 0.41 s.

### Instrumentation

| Macro | Effect |
| --- | --- |
| `ROOKS_PRINT` | Print every new best configuration. See the note above for the format. |
| `ROOKS_MONITOR` | Spawn a thread that prints the live configuration once a second. Needs `-lpthread`. |
| `R_COUNTER` | Print, at exit, how many branches each optimisation cut. Caveats: slot 0 counts *completed* configurations, not a prune; slots 1 and 2 always read 0, because the `R_OPTIM1`/`R_OPTIM2` conditions are caught earlier by the inline `OPTIM1_CHECK`/`OPTIM2_CHECK` inside `backtrack`, which do not increment; slot 3 is never written. |

## Reference values

Maximum number of rooks. Times are one core of a Ryzen-class laptop under WSL,
`-O2 -DNDEBUG`, with `R_OPTIM1`, `2`, `3`, `4` and `6`:

| `N` | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| max rooks | 1 | 2 | 6 | 9 | 12 | 18 | 22 | 32 | 36 |
| time | — | 0.00 s | 0.00 s | 0.00 s | 0.01 s | 0.01 s | 0.03 s | 0.90 s | 6 m 51 s |

At `-O3 -march=native`, `N = 8` drops to 0.76 s; adding the unproved
`R_OPTIM5` roughly halves it again.

`N = 1 … 4` were cross-checked against an independent brute force written from
the problem statement, which also gives `1, 2, 6, 9`. `N = 1` is not produced
by this program (see below). `N = 2 … 8` agree across every
correctness-preserving combination of the options that was tried, including
`R_OPTIM3` alone, which depends on nothing else.

## Known issues

1. **`N = 1` fails the assertion `i < N`.** `recurse_initial_line` lacks the
   `i == 0` termination check that `recurse` has, so the initial line of a
   single-slice cube recurses to `i = -1`. The answer is 1.
2. **`N ≤ 31`.** The projections are `uint32_t` and the code does `1 << i` on
   `int`, so `N = 32` would overflow. Runtime is the real limit long before
   that. `N_BITFIELD2` is declared but unused.
3. ~~**The combination of symmetry breakers is not formally justified
   here.**~~ **Settled: `R_OPTIM1`, `R_OPTIM2`, `R_OPTIM3` and `R_OPTIM6` do
   compose, for every `N`.** The doubt was about `R_OPTIM6`, which breaks ties
   in the `x`-ordering using `proj_y_x[]` — a quantity that a `y`-permutation,
   and hence `R_OPTIM2`, does change. [`docs/optim6-optim2.md`][o62] reduces
   the four of them to a statement about 0/1 matrices and proves it: every
   `N × N` 0/1 matrix admits a row and a column permutation for which the rows
   are sorted by `(cardinality, row-as-integer)` and the columns by
   `column-as-integer` *at the same time*, because row sums are invariant under
   column permutations and `sum M[i][j] 2^(i+j)` is then a monovariant. The
   statement was also verified exhaustively for every 0/1 matrix up to
   `N = 6`, and the solver was checked at `N = 4` and `N = 5` to still reach
   every orbit of optimal configurations. So no reference value is affected and
   nothing has to be dropped. Still open, and unrelated: `R_OPTIM4`'s
   dependency on `R_OPTIM1` (see the table above) and `R_OPTIM5`'s lemma.

[o62]: docs/optim6-optim2.md

## The removed `R_OPTIM1` skip-ahead

Between commits b1d4947 and HEAD, `backtrack` carried a second, inline copy of
the `R_OPTIM1` test which, instead of simply pruning, rewrote `j` to 0 so as to
jump straight to the end of the current line. It was removed, because:

* **It was broken.** It left `j = 0` while dispatching through
  `Worker_next<on_initial_line, on_initial_column, true>`, whose
  `on_initial_column` had been instantiated for the *original* `j`. When that
  was `true` (`j` was `N-1`), `Worker_next<false, true, true>` did
  `backtrack(c, i, j - 1)` and `j - 1` underflowed `size_t`: an assertion
  failure for every `N ≥ 2`, or an out-of-bounds access under `-DNDEBUG`.
  b1d4947's parent 16a46df was fine.
* **It read out of bounds.** Being inline in `backtrack`, it ran for every
  instantiation, including `on_initial_line`, where `i == N-1` and so
  `cardinal_x[i+1]` read `cardinal_x[N]`, one past the array, aliasing the
  `max_z` field. With `R_OPTIM3` on, `max_z` grows and the condition never
  fires, so the bug hid; with `R_OPTIM3` off, `max_z` stays 1 and the first
  slice was silently capped at one rook, so `R_OPTIM1 + R_OPTIM4` returned `N`
  instead of the optimum (3, 4, 5, 6 for `N = 3 … 6`).
* **It bought nothing.** At `N = 8`, `-O3 -march=native`, best of ten: 0.76 s
  without it, 0.78–0.84 s with it (repaired, in either b1d4947's or a888490's
  form). At `N = 9`, `-O2`: 6 m 51 s without it against 7 m 28 s with it.
  a888490 reported "sometimes > 30 percents" for turning the dispatch
  into a `goto`, and b1d4947 reported 1% for the skip itself; neither
  reproduces today. The likeliest explanation is a3f0fe9, which inlined
  `backtrack_pillar` into `backtrack`, together with the `R_OPTIM2` early
  skip from 16a46df now cutting those branches first.

Deleting it reverts b1d4947, a888490 and 0716946 in effect. `R_OPTIM1` itself
is untouched: the prune in `Worker_next<false, false, false>` is the original
pre-b1d4947 implementation, so the symmetry break — and with it the validity
of `R_OPTIM4`'s bound — still holds.
