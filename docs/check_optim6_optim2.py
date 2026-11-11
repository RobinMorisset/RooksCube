#!/usr/bin/env python3
"""Exhaustively check that R_OPTIM1 + R_OPTIM6 + R_OPTIM2 compose.

The three symmetry breakers of main.cpp constrain only the 0/1 matrix
M[x][y] = (heights[x][y] != 0) of occupied pillars.  Writing

    s_i = sum_j M[i][j]                 (cardinal_x[i])
    r_i = sum_j M[i][j] * 2**j          (proj_y_x[i] as an integer)
    c_j = sum_i M[i][j] * 2**i          (proj_x_y[j] as an integer)

the surviving configurations are exactly those with

    (A)  (s_i, r_i) lexicographically non-decreasing in i   [OPTIM1 + OPTIM6]
    (B)  c_j non-decreasing in j                            [OPTIM2]

so the solver is complete iff every N x N 0/1 matrix has a row permutation and
a column permutation making (A) and (B) hold *at the same time*.  The circular
part is that r depends on the column order and c on the row order.

This script checks that statement for every matrix up to row permutation
(which is enough: the statement is invariant under row and column
permutations).  Two independent checkers:

  * `witness` runs the alternating sort "sort rows by (s, r); sort columns by
    c; repeat".  Whenever it stops it returns a matrix satisfying (A) and (B),
    i.e. a witness, so success proves the statement for that matrix.  The
    write-up in docs/optim6-optim2.md proves it always stops, by showing that
    Phi = sum_ij M[i][j] * 2**(i+j) strictly increases at every step after the
    first row sort.

  * `exists_brute` enumerates all N! column permutations and, for each, the
    forced row order, and reports whether any pair satisfies (A) and (B).
    Used as a cross-check for small N and as the fallback if `witness` ever
    fails.

Usage: check_optim6_optim2.py [max_N] [--brute-upto K]
"""
import itertools
import sys


def popcount(v):
    return bin(v).count("1")


def row_key(v):
    """OPTIM1's key first, then OPTIM6's tie-break."""
    return (popcount(v), v)


def col_values(N, rows):
    cols = [0] * N
    for i, v in enumerate(rows):
        for j in range(N):
            if v >> j & 1:
                cols[j] |= 1 << i
    return cols


def permute_columns(N, rows, order):
    """New column k is old column order[k]."""
    out = []
    for v in rows:
        w = 0
        for k in range(N):
            if v >> order[k] & 1:
                w |= 1 << k
        out.append(w)
    return out


def phi(rows):
    return sum(v << i for i, v in enumerate(rows))


def witness(N, rows, cap=1000):
    """Alternating sort.  Returns a matrix satisfying (A) and (B), or None."""
    rows = sorted(rows, key=row_key)
    for _ in range(cap):
        cols = col_values(N, rows)
        if all(cols[j] <= cols[j + 1] for j in range(N - 1)):
            return rows  # rows are sorted by row_key by construction
        order = sorted(range(N), key=lambda j: cols[j])
        rows = sorted(permute_columns(N, rows, order), key=row_key)
    return None


def exists_brute(N, rows):
    """Try every column permutation; the row order is then forced."""
    for order in itertools.permutations(range(N)):
        cand = sorted(permute_columns(N, rows, order), key=row_key)
        cols = col_values(N, cand)
        if all(cols[j] <= cols[j + 1] for j in range(N - 1)):
            return cand
    return None


def check(N, brute=False):
    total = 0
    worst_iters = 0
    failures = []
    disagreements = []
    for rows in itertools.combinations_with_replacement(range(1 << N), N):
        total += 1
        w = witness(N, rows)
        if w is None:
            failures.append(rows)
        if brute:
            b = exists_brute(N, rows)
            if (w is None) != (b is None):
                disagreements.append(rows)
    print("N=%d: %d matrices up to row permutation" % (N, total))
    print("      alternating sort found a witness for %d of them"
          % (total - len(failures)))
    if failures:
        print("      !! NO WITNESS for %d matrices, e.g. %r"
              % (len(failures), failures[0]))
        for rows in failures[:3]:
            print("         brute force:", exists_brute(N, rows))
    if brute:
        print("      brute force over all %d column permutations agrees: %s"
              % (len(list(itertools.permutations(range(N)))),
                 "yes" if not disagreements else "NO (%r)" % disagreements[:3]))
    return not failures and not disagreements


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    max_n = int(args[0]) if args else 5
    brute_upto = 4
    if "--brute-upto" in sys.argv:
        brute_upto = int(sys.argv[sys.argv.index("--brute-upto") + 1])
    ok = True
    for n in range(1, max_n + 1):
        ok &= check(n, brute=(n <= brute_upto))
    print("ALL OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
