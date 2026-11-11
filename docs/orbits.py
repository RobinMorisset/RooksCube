#!/usr/bin/env python3
"""Check that R_OPTIM1+2+3+6 still reaches every orbit of optimal solutions.

Reads two dumps produced by docs/dump_optima.sh: the ground truth (R_OPTIM3
alone, i.e. every optimal configuration, one per orbit under permutation of
the heights) and the run with every symmetry breaker under test.  Then

  * validates every dumped configuration against the problem statement,
    written out from scratch here (so this is also an independent check that
    the solver's incremental constraint propagation is right),
  * groups the ground-truth configurations into orbits under the group
    S_N(x) x S_N(y) x S_N(z) that the three symmetry breakers quotient by,
  * checks that the symmetry-broken dump contains at least one configuration
    from every one of those orbits.

Usage: orbits.py <N> <optima_truth.txt> <optima_sym.txt>
"""
import itertools
import sys


def parse(path, N):
    """Each configuration is N lines of N tokens, then a 'result:' line."""
    configs = []
    rows = []
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        if line.startswith("result:"):
            assert len(rows) == N, rows
            configs.append(tuple(rows))
            rows = []
            continue
        if line.startswith("-----"):
            continue
        toks = line.split()
        assert len(toks) == N, line
        row = []
        for t in toks:
            if t == "*":
                row.append(0)
            elif t.isdigit():
                row.append(int(t))
            else:
                row.append(ord(t) - ord("A") + 10)
        rows.append(tuple(row))
    assert not rows
    return configs


def rooks(h, N):
    """(x, y, z) of every rook; heights in the dump are 1-based."""
    return [(x, y, h[x][y] - 1) for x in range(N) for y in range(N)
            if h[x][y]]


def valid(h, N):
    rk = rooks(h, N)
    s = set(rk)
    # 1. no two rooks on a common axis-parallel line
    for a, b in itertools.combinations(rk, 2):
        if sum(1 for k in range(3) if a[k] == b[k]) >= 2:
            return False
    # 2. no cell has all three of its lines occupied by some other rook
    by_yz, by_xz, by_xy = {}, {}, {}
    for (x, y, z) in rk:
        by_yz.setdefault((y, z), []).append(x)
        by_xz.setdefault((x, z), []).append(y)
        by_xy.setdefault((x, y), []).append(z)
    for x in range(N):
        for y in range(N):
            for z in range(N):
                a = any(v != x for v in by_yz.get((y, z), ()))
                b = any(v != y for v in by_xz.get((x, z), ()))
                c = any(v != z for v in by_xy.get((x, y), ()))
                if a and b and c:
                    return False
    return True


def card(h, N):
    return sum(1 for x in range(N) for y in range(N) if h[x][y])


def z_canon(h, N):
    """Relabel heights by order of first appearance (what R_OPTIM3 enforces)."""
    seen = {}
    out = []
    for x in range(N):
        row = []
        for y in range(N):
            v = h[x][y]
            if v and v not in seen:
                seen[v] = len(seen) + 1
            row.append(seen[v] if v else 0)
        out.append(tuple(row))
    return tuple(out)


def swap_rows(h, i):
    rows = list(h)
    rows[i], rows[i + 1] = rows[i + 1], rows[i]
    return tuple(rows)


def swap_cols(h, j, N):
    out = []
    for row in h:
        r = list(row)
        r[j], r[j + 1] = r[j + 1], r[j]
        out.append(tuple(r))
    return tuple(out)


def orbits_by_union_find(configs, N):
    """Group z-canonical configurations into orbits under S_N(x) x S_N(y).

    The ground-truth dump is the whole set of optimal configurations modulo
    permutation of the heights, so it is closed under row and column
    permutations once each image is re-canonicalised in z.  Adjacent
    transpositions generate both symmetric groups, so union-find over those
    2*(N-1) generators gives exactly the orbits -- far cheaper than minimising
    over all (N!)^2 pairs.
    """
    index = {h: k for k, h in enumerate(configs)}
    parent = list(range(len(configs)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    for k, h in enumerate(configs):
        for i in range(N - 1):
            for img in (swap_rows(h, i), swap_cols(h, i, N)):
                img = z_canon(img, N)
                other = index.get(img)
                assert other is not None, (
                    "ground-truth dump is not closed under the symmetry "
                    "group -- it is not the full set of optima", h, img)
                union(k, other)
    return index, [find(k) for k in range(len(configs))]


def matrix_conditions(h, N):
    """(A) from R_OPTIM1+R_OPTIM6 and (B) from R_OPTIM2, on the 0/1 matrix."""
    s = [sum(1 for y in range(N) if h[x][y]) for x in range(N)]
    r = [sum(1 << y for y in range(N) if h[x][y]) for x in range(N)]
    c = [sum(1 << x for x in range(N) if h[x][y]) for y in range(N)]
    a = all((s[i], r[i]) <= (s[i + 1], r[i + 1]) for i in range(N - 1))
    b = all(c[j] <= c[j + 1] for j in range(N - 1))
    return a, b


def main():
    N = int(sys.argv[1])
    truth = parse(sys.argv[2], N)
    sym = parse(sys.argv[3], N)
    opt = max(card(h, N) for h in truth)
    truth = [h for h in truth if card(h, N) == opt]
    sym = [h for h in sym if card(h, N) == opt]
    print("N=%d, optimum %d rooks" % (N, opt))

    bad = [h for h in truth + sym if not valid(h, N)]
    print("  %d + %d configurations dumped, %d invalid"
          % (len(truth), len(sym), len(bad)))
    assert not bad, bad[:1]

    configs = sorted(set(z_canon(h, N) for h in truth))
    print("  %d optima up to permutation of the heights" % len(configs))
    index, root = orbits_by_union_find(configs, N)
    norbits = len(set(root))
    print("  they form %d orbits under S_N(x) x S_N(y) x S_N(z)" % norbits)

    hit = set()
    for h in sym:
        a, b = matrix_conditions(h, N)
        assert a and b, ("dumped config violates (A) or (B)", h, a, b)
        k = index.get(z_canon(h, N))
        assert k is not None, ("symmetry-broken run produced a "
                               "configuration absent from the ground truth", h)
        hit.add(root[k])
    print("  symmetry-broken run kept %d of the %d orbits, lost %d"
          % (len(hit), norbits, norbits - len(hit)))
    if len(hit) != norbits:
        lost = [h for k, h in enumerate(configs) if root[k] not in hit]
        print("  !! LOST ORBITS, e.g.:")
        for row in lost[0]:
            print("    ", row)
        return 1
    print("  OK: every orbit of optima survives R_OPTIM1+2+3+6")
    return 0


if __name__ == "__main__":
    sys.exit(main())
