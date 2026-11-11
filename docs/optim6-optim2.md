# `R_OPTIM6` and `R_OPTIM2` do compose

**Result.** The three symmetry breakers `R_OPTIM1` (sort the `x`-slices by
cardinality), `R_OPTIM6` (break ties in that order by `proj_y_x[]`) and
`R_OPTIM2` (sort the `y`-slices by `proj_x_y[]`) can all be enabled together
without losing any optimum, for every `N`. This is proved in section 4, and
the key statement is also verified exhaustively up to `N = 6` (section 3) and
checked end-to-end against the solver at `N = 4` and `N = 5` (section 5).

Consequence: item 3 of the README's *Known issues* is settled in the
affirmative. All of the reference values `N = 1 … 9` stand, nothing has to be
dropped, and there is no performance price to pay.

The same proof covers every sub-combination — `R_OPTIM1` + `R_OPTIM2`,
`R_OPTIM1` + `R_OPTIM6`, and each one on its own — since the canonical
configuration it produces satisfies all three conditions at once, hence any
subset of them. (It would also cover `R_OPTIM6` without `R_OPTIM1`, but
`main.cpp` refuses to build that combination, reasonably: a tie-break on an
order that is not being enforced only costs pruning.)

It does **not** cover `R_OPTIM4` (whose bound still needs `R_OPTIM1` — that
dependency is a separate matter, documented in the README) or `R_OPTIM5`
(still unproved).

## 1. What the three prunes actually constrain

`backtrack` fills the `N²` pillars in a fixed order: slices `x = N-1` down to
`0`, and inside a slice `y = N-1` down to `0`. So at pillar `(i, j)` the
slices `i+1 … N-1` are complete and, inside slice `i`, the pillars
`j+1 … N-1` are decided.

All three prunes read only `cardinal_x[]`, `proj_y_x[]` and `proj_x_y[]`, and
all three of those are functions of *which pillars hold a rook*, not of the
heights. So the prunes constrain only the `N × N` 0/1 matrix

    M[x][y] = (heights[x][y] != 0)

Write, for a complete configuration,

    s_i = sum_j M[i][j]            = cardinal_x[i]
    r_i = sum_j M[i][j] * 2**j     = proj_y_x[i] read as an integer
    c_j = sum_i M[i][j] * 2**i     = proj_x_y[j] read as an integer

Note that the two bit weightings are consistent: a row weights column `j` by
`2**j` and a column weights row `i` by `2**i`. The final conditions the prunes
are aiming at are then

* **(A)** `(s_i, r_i) <= (s_{i+1}, r_{i+1})` lexicographically, for all
  `i < N-1` — the rows sorted by cardinality, ties broken by the row read as
  an integer. `R_OPTIM1` is the first component, `R_OPTIM6` the second.
* **(B)** `c_j <= c_{j+1}` for all `j < N-1` — the columns sorted as
  integers. This is `R_OPTIM2`. (`proj_x_y[N] = ~0` is a sentinel, so the test
  at `j = N-1` is vacuous, as it should be: there is no column `N`.)

The prunes are applied *incrementally*, on partial state, so (A) and (B) are
not literally what is tested. What matters for correctness is only one
direction:

> **Lemma 1.** If any of the three prunes fires on the way to a complete
> configuration, then that complete configuration violates (A) or (B).

*Proof.* Each case uses that the search only ever adds rooks to the slice it
is currently in, and that slice `i+1` (resp. column `j+1`) is already final.

* `R_OPTIM1` fires when `cardinal_x[i] > cardinal_x[i+1]`. The left side only
  grows as slice `i` is filled, and the right side is already final, so
  finally `s_i > s_{i+1}`: (A) fails.

* `R_OPTIM2` fires when `proj_x_y[j] > proj_x_y[j+1]`. At that moment both
  columns contain only bits of rows `>= i` (rows `< i` are undecided, and in
  row `i` column `j+1` was visited before column `j`), so both values are
  multiples of `2**i` and the strict inequality gives
  `proj_x_y[j] >= proj_x_y[j+1] + 2**i`. The bits still to be decided, rows
  `< i`, add less than `2**i` to each, so finally `c_j > c_{j+1}`: (B) fails.

* `R_OPTIM6` fires when `cardinal_x[i] == cardinal_x[i+1]` and
  `proj_y_x[i] > proj_y_x[i+1]`. If slice `i` ends up with more rooks then
  `s_i > s_{i+1}` and (A) fails as above. Otherwise slice `i` receives no
  further rook, so the partial `proj_y_x[i]` is already the final `r_i`, and
  `r_i > r_{i+1}` with `s_i = s_{i+1}`: (A) fails. ∎

So **every complete configuration whose matrix satisfies (A) and (B) is
reached by the search.** The converse fails mildly: `R_OPTIM1` and `R_OPTIM6`
are only tested right after a rook is placed at a pillar with `i <= N-2` and
`j <= N-2`, so for instance "slice `i+1` empty, slice `i` holding a single
rook at `y = N-1`" is never caught, and a few configurations violating (A)
survive. That costs a little pruning and nothing else.

`R_OPTIM3` factors out cleanly, as the README already says: it constrains only
the order in which *heights* are first used, a relabelling of `z` leaves `M`
untouched, and (A) and (B) depend only on `M`. So any configuration can be put
into `R_OPTIM3`'s canonical form — relabel the heights by order of first use —
without disturbing (A) or (B).

## 2. The statement to prove

Permuting the `x`-slices, the `y`-slices or the heights maps a valid
configuration to a valid configuration with the same number of rooks; the
group is `S_N(x) × S_N(y) × S_N(z)`. With Lemma 1 and the `R_OPTIM3` remark,
the solver is complete iff every orbit contains a configuration satisfying (A)
and (B), that is, iff:

> **Theorem.** Every `N × N` 0/1 matrix admits a row permutation and a column
> permutation such that the permuted matrix satisfies (A) and (B)
> simultaneously.

The circularity is the crux: `r_i` depends on the column order and `c_j`
depends on the row order, so the two sorts fight each other.

## 3. Exhaustive verification

`docs/check_optim6_optim2.py` and `docs/check_optim6_optim2.cpp` (same
algorithms, independent implementations) enumerate every `N × N` 0/1 matrix up
to row permutation — enough, since the property is invariant under row and
column permutations — as every non-decreasing `N`-tuple of row bitmasks, that
is `C(2**N + N - 1, N)` matrices. For each they run the **alternating sort**

    sort the rows by (s, r);  sort the columns by c;  repeat

whose stopping point is by construction a matrix satisfying (A) and (B), i.e.
a witness. The Python script additionally cross-checks, for `N <= 5`, against
a brute force over all `N!` column orders (for each of which the row order is
then forced, up to irrelevant ties between identical rows).

| `N` | matrices up to row perm. | witness found for | brute force agrees | worst case |
| --- | --- | --- | --- | --- |
| 1 | 2 | all | yes | 0 column sorts |
| 2 | 10 | all | yes | 1 |
| 3 | 120 | all | yes | 1 |
| 4 | 3 876 | all | yes | 2 |
| 5 | 376 992 | all | yes | 3 |
| 6 | 119 877 472 | all | not run | 4 |

No counterexample exists at any `N <= 6`. `N = 6` takes 22 s on one core with
the C++ version:

```bash
python docs/check_optim6_optim2.py 5 --brute-upto 5
```

```bash
g++ -std=c++11 -O3 -march=native -DN=6 -o check6 docs/check_optim6_optim2.cpp && ./check6
```

`N = 7` would be `C(134, 7) ≈ 1.3 × 10¹¹` matrices and was not attempted; the
proof below makes it unnecessary.

## 4. Proof

**Theorem.** For every `M ∈ {0,1}^(N×N)` there are permutations `σ, τ ∈ S_N`
such that `M'[i][j] = M[σ(i)][τ(j)]` satisfies (A) and (B).

**Proof.** Let

    Phi(M) = sum_{i,j} M[i][j] * 2**(i+j) = sum_i r_i * 2**i = sum_j c_j * 2**j

Let `G` be the set of pairs `(σ, τ)` for which the row sums of `M'` are
non-decreasing. A column permutation does not change row sums, so `G` is
exactly `{σ : σ sorts the row-sum vector} × S_N`: non-empty, and finite. Pick
`(σ, τ) ∈ G` maximising `Phi(M')`, and write `M'`, `s`, `r`, `c` for the
corresponding matrix and vectors.

*(B) holds.* Suppose `c_j > c_{j+1}`. Swapping columns `j` and `j+1` leaves
the row sums alone, so it stays inside `G`, and — using
`Phi = sum_j c_j 2**j`, of which the swap only moves those two terms — it
changes `Phi` by

    (c_j - c_{j+1}) * (2**(j+1) - 2**j) > 0

contradicting maximality.

*(A) holds.* The sums are non-decreasing because `(σ, τ) ∈ G`. Suppose
`s_i = s_{i+1}` but `r_i > r_{i+1}`. Swapping rows `i` and `i+1` keeps the row
sums non-decreasing — the two being swapped are equal — so it stays inside
`G`, and it changes `Phi = sum_i r_i 2**i` by

    (r_i - r_{i+1}) * (2**(i+1) - 2**i) > 0

again contradicting maximality. ∎

The proof is constructive, and the construction is exactly the alternating
sort of section 3: after the first row sort the matrix is in `G`, and every
later step — a column sort, or a row sort which by then only reorders rows
*within* blocks of equal sum — keeps it in `G` and strictly increases the
integer `Phi` unless it changes nothing at all. So the alternating sort
terminates, at a matrix satisfying (A) and (B).

### Why this is not automatic

The naive version of the argument fails. Sorting the rows by the key `(s, r)`
does *not* maximise `Phi` over row permutations — sorting them by `r` alone
would. Take two rows at `N = 3`, `100` (`r = 4`, `s = 1`) and `011` (`r = 3`,
`s = 2`): the key puts them in the order `100`, `011`, for
`Phi = 4·1 + 3·2 = 10`, while the other order gives `3·1 + 4·2 = 11`. So
`Phi` is not a monovariant for a *global* sort by the `R_OPTIM1`/`R_OPTIM6`
key, and "sort rows, sort columns, repeat" could a priori cycle for ever.

What rescues it is that **row sums are invariant under column permutations**.
That fixes the partition of the rows into equal-sum blocks, and the order of
those blocks, once and for all; the tie-break then only ever permutes rows
inside a block, where the key degenerates to `r` and the plain `Phi`
rearrangement applies.

So the result is specific to these keys, and porting it to a MAXSAT / PB / ILP
encoding needs care:

* A primary row key that is *not* invariant under the column permutation —
  anything a `y`-permutation could reorder across blocks — breaks the
  argument. `cardinal_x[]` is fine. (Rows by `r` alone with columns by `c`
  alone is also fine, by the plain `Phi` argument, with no blocks needed.)
* The two bit weightings must agree in direction, as they do here: the proof
  uses `2**(i+j)`, so it needs both orders ascending with the same bit order
  on both axes.
* Sorting the columns by `(column sum, c)` rather than by `c` alone is fine
  too, by the same block argument applied on the other axis — column sums are
  invariant under row permutations, so both block structures are fixed
  simultaneously.

## 5. End-to-end check on the solver

Sections 1–4 settle the mathematics. What they do not exercise is the actual
incremental bookkeeping in `main.cpp`, which Lemma 1 covers only on paper, so
it is worth checking against the running solver.

`docs/dump_optima.sh` builds a copy of `main.cpp` with the "is this a new
best" test replaced by "does this reach `DUMP_MIN` rooks" and `update_best()`
by `print_config()` — so `best_card` never moves and *every* configuration
reaching the optimum is printed, not only the improving ones — and dumps all
optima twice: once with `R_OPTIM3` alone (no `x`/`y` symmetry breaking at all,
and no `R_OPTIM4` either, since its bound needs `R_OPTIM1`), and once with
`R_OPTIM1` + `R_OPTIM2` + `R_OPTIM3` + `R_OPTIM6`.

The threshold deliberately does not go through `R_MIN`. `R_MIN` only seeds
`best_card`, which nothing but `R_OPTIM4` consults for pruning, so it would
not have bounded these runs in any case — and `main.cpp` now refuses to build
`R_MIN` without `R_OPTIM4`, whose bound needs `R_OPTIM1`, which is exactly the
symmetry breaking the ground-truth run must not have. Both runs are therefore
full searches, which is the real reason `N = 6` is out of reach below.
`docs/orbits.py` then validates every dumped configuration against the two
problem constraints, written out from scratch, groups the ground truth into
orbits, and checks that the symmetry-broken dump covers every orbit. The
orbits are computed by union-find over adjacent row and column transpositions,
re-canonicalising the heights at each step, which is far cheaper than
minimising over all `(N!)²` pairs.

```bash
sh docs/dump_optima.sh 5 12 /tmp/dump
```

```bash
python docs/orbits.py 5 /tmp/dump/optima_truth_5.txt /tmp/dump/optima_sym_5.txt
```

| `N` | optimum | optima mod `z`-permutation | orbits | orbits kept | configurations kept |
| --- | --- | --- | --- | --- | --- |
| 4 | 9 | 192 | 1 | 1 | 2 |
| 5 | 12 | 113 400 | 14 | 14 | 54 |

Nothing is lost, every dumped configuration is valid, and every configuration
the symmetry-broken run produces does satisfy (A) and (B) — a direct check of
Lemma 1 against the code.

`N = 6` was not done this way: the ground-truth run (`R_OPTIM3` alone, hence
no `R_OPTIM4`) does not finish in a reasonable time. For `N = 6` the symmetry
argument is covered by the exhaustive matrix check of section 3 instead.

### The optima themselves, by combination

Finally, the plain check that the reported optimum does not move. Every
combination below includes `R_OPTIM3` (without it even `N = 5` is slow); the
`N = 6 … 8` runs need `R_OPTIM4`, which needs `R_OPTIM1`, so the weaker
combinations are only run up to `N = 5`. `>` means the run was cut off at that
many seconds, not that it gave a different answer.

| combination | `N=2` | `N=3` | `N=4` | `N=5` | `N=6` | `N=7` | `N=8` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `3` | 2 | 6 | 9 | 12 | | | |
| `1`+`3` | 2 | 6 | 9 | 12 | | | |
| `2`+`3` | 2 | 6 | 9 | 12 | | | |
| `1`+`2`+`3` | 2 | 6 | 9 | 12 | | | |
| `1`+`3`+`6` | 2 | 6 | 9 | 12 | | | |
| `1`+`2`+`3`+`6` | 2 | 6 | 9 | 12 | | | |
| `1`+`3`+`4` | | | | | 18 | 22 | >60 s |
| `1`+`2`+`3`+`4` | | | | | 18 | 22 | 32 |
| `1`+`3`+`4`+`6` | | | | | 18 | 22 | >60 s |
| `1`+`2`+`3`+`4`+`6` | | | | | 18 | 22 | 32 |

Every run that finished agrees with the reference values. In particular
`R_OPTIM2` and `R_OPTIM6` change the answer neither separately nor together,
and dropping `R_OPTIM2` is what makes `N = 8` slow — `R_OPTIM6` is not
carrying the runtime.

## 6. Reproducing

| File | What it does |
| --- | --- |
| `docs/check_optim6_optim2.py` | The theorem, exhaustively, `N = 1 … 5`: alternating sort plus brute force over all `N!` column orders. |
| `docs/check_optim6_optim2.cpp` | Same, one `N` per build, used for `N = 6` (22 s). |
| `docs/dump_optima.sh` | Dumps every optimal configuration, with and without the symmetry breakers. Does not modify `main.cpp`. |
| `docs/orbits.py` | Validates the dumps, computes the orbits, checks that none is lost. |
