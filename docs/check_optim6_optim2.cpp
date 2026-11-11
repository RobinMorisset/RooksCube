/* Exhaustive check that R_OPTIM1 + R_OPTIM6 + R_OPTIM2 compose, for one N.
 *
 * Same statement and same two checkers as docs/check_optim6_optim2.py, which
 * documents them; this is the C++ version, needed because N = 6 has
 * C(2^6 + 5, 6) = 119 877 472 matrices up to row permutation.
 *
 *   g++ -std=c++11 -O3 -march=native -DN=6 -o check6 docs/check_optim6_optim2.cpp
 *   ./check6
 *
 * Enumerates every non-decreasing N-tuple of row bitmasks -- i.e. every
 * N x N 0/1 matrix up to row permutation, which is enough because the
 * property checked is invariant under row and column permutations -- and for
 * each runs the alternating sort "sort rows by (popcount, value); sort columns
 * by column-value; repeat".  A success is a witness, so the statement holds
 * for that matrix.  Matrices where the alternating sort does not stop within
 * ITER_CAP steps fall back to brute force over all N! column permutations and
 * are printed.
 */
#include <stdio.h>
#include <stdint.h>
#include <algorithm>

#ifndef N
#error "define N"
#endif

#define ITER_CAP 64

typedef uint32_t Row;

static inline bool row_less(Row a, Row b) {
	int pa = __builtin_popcount(a), pb = __builtin_popcount(b);
	if (pa != pb) return pa < pb;
	return a < b;
}

/* columns as integers: bit i of column j is M[i][j] */
static inline void col_values(const Row *rows, Row *cols) {
	for (int j = 0; j < N; j++) cols[j] = 0;
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++)
			if (rows[i] >> j & 1) cols[j] |= (Row) 1 << i;
}

static inline bool cols_sorted(const Row *cols) {
	for (int j = 0; j + 1 < N; j++)
		if (cols[j] > cols[j+1]) return false;
	return true;
}

static inline void permute_columns(Row *rows, const int *order) {
	for (int i = 0; i < N; i++) {
		Row v = rows[i], w = 0;
		for (int k = 0; k < N; k++)
			if (v >> order[k] & 1) w |= (Row) 1 << k;
		rows[i] = w;
	}
}

/* alternating sort; returns the number of column sorts used, or -1 */
static int witness(const Row *in, Row *out) {
	Row rows[N], cols[N];
	for (int i = 0; i < N; i++) rows[i] = in[i];
	std::sort(rows, rows + N, row_less);
	for (int it = 0; it < ITER_CAP; it++) {
		col_values(rows, cols);
		if (cols_sorted(cols)) {
			for (int i = 0; i < N; i++) out[i] = rows[i];
			return it;
		}
		int order[N];
		for (int j = 0; j < N; j++) order[j] = j;
		std::stable_sort(order, order + N,
				[&cols](int a, int b) { return cols[a] < cols[b]; });
		permute_columns(rows, order);
		std::sort(rows, rows + N, row_less);
	}
	return -1;
}

static bool exists_brute(const Row *in) {
	int order[N];
	for (int j = 0; j < N; j++) order[j] = j;
	do {
		Row rows[N], cols[N];
		for (int i = 0; i < N; i++) rows[i] = in[i];
		permute_columns(rows, order);
		std::sort(rows, rows + N, row_less);
		col_values(rows, cols);
		if (cols_sorted(cols)) return true;
	} while (std::next_permutation(order, order + N));
	return false;
}

int main(void) {
	Row rows[N], out[N];
	uint64_t total = 0, failures = 0, brute_ok = 0;
	int worst = 0;
	for (int i = 0; i < N; i++) rows[i] = 0;
	for (;;) {
		total++;
		int it = witness(rows, out);
		if (it < 0) {
			failures++;
			bool b = exists_brute(rows);
			if (b) brute_ok++;
			if (failures <= 10) {
				printf("no witness:");
				for (int i = 0; i < N; i++) printf(" %u", rows[i]);
				printf("   brute force: %s\n", b ? "exists" : "NONE");
			}
		} else if (it > worst) {
			worst = it;
		}
		/* next non-decreasing tuple */
		int p = N - 1;
		while (p >= 0 && rows[p] == ((Row) 1 << N) - 1) p--;
		if (p < 0) break;
		Row v = rows[p] + 1;
		for (int i = p; i < N; i++) rows[i] = v;
	}
	printf("N=%d: %llu matrices up to row permutation\n",
			N, (unsigned long long) total);
	printf("      alternating sort found a witness for %llu of them\n",
			(unsigned long long) (total - failures));
	printf("      worst case: %d column sorts\n", worst);
	if (failures)
		printf("      !! %llu without a witness, %llu of those satisfiable "
				"by brute force\n", (unsigned long long) failures,
				(unsigned long long) brute_ok);
	printf("%s\n", failures ? "FAILED" : "ALL OK");
	return failures ? 1 : 0;
}
