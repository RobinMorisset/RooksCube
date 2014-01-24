#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>

#define N 7
#define N_BITFIELD uint_fast8_t
// TODO: also define a type for the indexes to check whether char/int is more
// efficient than size_t


struct Config {
	char heights[N][N];
	// the index corresponds to x, the bitfield to a 'pillar' (z)
	N_BITFIELD forbidden_z_x[N];
	N_BITFIELD forbidden_z_y[N];
	N_BITFIELD forbidden_y_x[N];
	N_BITFIELD forbidden_y_z[N];
	N_BITFIELD forbidden_x_y[N];
	N_BITFIELD forbidden_x_z[N];
	N_BITFIELD proj_z_x[N];
	N_BITFIELD proj_z_y[N];
	N_BITFIELD proj_y_z[N];
	N_BITFIELD proj_y_x[N];
	N_BITFIELD proj_x_y[N];
	N_BITFIELD proj_x_z[N];
	int k;
};

void backtrack(struct Config *, struct Config * b, size_t, size_t);

void backtrack_pillar(struct Config *c, struct Config * b,
		size_t i, size_t j, N_BITFIELD mask_x, N_BITFIELD mask_y,
		size_t i2, size_t j2) {
	N_BITFIELD mask_z;
	N_BITFIELD forbidden_z = c->forbidden_z_x[i] | c->forbidden_z_y[j];
	N_BITFIELD old_fzx, old_fzy, old_fyx, old_fyz, old_fxy, old_fxz;

	// Save to restore later
	old_fzx = c->forbidden_z_x[i]; // TODO: save only once per slice
	old_fyx = c->forbidden_y_x[i];
	old_fzy = c->forbidden_z_y[j];
	old_fxy = c->forbidden_x_y[j];
	assert(i < N);
	assert(j < N);
	c->k++;

	for (size_t k = 0; k < N; k++) {
		// First check that this height is allowed.
		// Could be further optimised with the __builtin_ffsll
		mask_z = 1 << k;
		if((mask_z & forbidden_z)
				|| (mask_x & c->forbidden_x_z[k])
				|| (mask_y & c->forbidden_y_z[k]))
			continue;

		// Save to restore later
		old_fyz = c->forbidden_y_z[k];
		old_fxz = c->forbidden_x_z[k];

		// Add the rook
		c->heights[i][j] = k+1; // because 0 is no rook
		c->proj_z_x[i] |= mask_z;
		c->proj_z_y[j] |= mask_z;
		c->proj_y_z[k] |= mask_y;
		c->proj_y_x[i] |= mask_y;
		c->proj_x_z[k] |= mask_x;
		c->proj_x_y[i] |= mask_x;
		c->forbidden_z_x[i] |= c->proj_z_y[j];
		c->forbidden_z_y[j] |= c->proj_z_x[i];
		c->forbidden_y_x[i] |= c->proj_y_z[k];
		c->forbidden_y_z[k] |= c->proj_y_x[i];
		c->forbidden_x_y[j] |= c->proj_x_z[k];
		c->forbidden_x_z[k] |= c->proj_x_y[j];

		// Recursive call to backtrack
		backtrack(c, b, i2, j2);

		// Restore old values
		c->forbidden_z_x[i] = old_fzx;
		c->forbidden_z_y[j] = old_fzy;
		c->forbidden_y_x[i] = old_fyx;
		c->forbidden_y_z[k] = old_fyz;
		c->forbidden_x_y[j] = old_fxy;
		c->forbidden_x_z[k] = old_fxz;
		c->proj_z_x[i] ^= mask_z;
		c->proj_z_y[j] ^= mask_z;
		c->proj_y_z[k] ^= mask_y;
		c->proj_y_x[i] ^= mask_y;
		c->proj_x_z[k] ^= mask_x;
		c->proj_x_y[j] ^= mask_x;
	}

	// Finally try leaving the pillar empty
	c->heights[i][j] = 0;
	c->k--;
	backtrack(c, b, i2, j2);
}

void print_config(struct Config * c) {
	for (size_t i = 0; i < N; i++) {
		for (size_t j = 0; j < N; j++) {
			if (c->heights[i][j])
				printf("%i ", c->heights[i][j]);
			else
				printf("* ");
		}
		printf("\n");
	}

	printf("%i\n\n", c->k);
}

void backtrack(struct Config *c, struct Config *b, size_t i, size_t j) {
	int i2, j2;
	N_BITFIELD mask_x, mask_y;

	/* If we are beyond the end, stop */
	if (i == N) {
		if (c->k > b->k) {
			*b = *c;
			print_config(c);
		}
		return;
	}

	/* Getting the coordinates for the recursive call */
	if (j == N - 1) {
		i2 = i+1;
		j2 = 0;
	} else {
		i2 = i;
		j2 = j+1;
	}

	mask_x = 1 << i;
	mask_y = 1 << j;
	assert(i < N);
	assert(j < N);
	if ((c->forbidden_y_x[i] & mask_y) || (c->forbidden_x_y[j] & mask_x))
		backtrack(c, b, i2, j2);
	else
		backtrack_pillar(c, b, i, j, mask_x, mask_y, i2, j2);
}

int main(int argc, char* argv[])
{
	struct Config c;
	struct Config b;

	/* Initialisation */
	for (size_t i = 0; i < N; i++) {
		for (size_t j = 0; j < N; j++) {
			c.heights[i][j] = 0;
		}
		c.forbidden_z_x[i] = 0;
		c.forbidden_z_y[i] = 0;
		c.forbidden_y_x[i] = 0;
		c.forbidden_y_z[i] = 0;
		c.forbidden_x_y[i] = 0;
		c.forbidden_x_z[i] = 0;
		c.proj_z_x[i] = 0;
		c.proj_z_y[i] = 0;
		c.proj_y_z[i] = 0;
		c.proj_y_x[i] = 0;
		c.proj_x_y[i] = 0;
		c.proj_x_z[i] = 0;
	}
	c.k = 0;
	b.k = 0;

	/* Backtracking */
	backtrack(&c, &b, 0, 0);

	print_config(&b);
	printf("\n%i", b.k);
	return 0;
}
// c->forbidden_z_x[i] |= c->proj_z_y[j];
// c->forbidden_z_y[j] |= c->proj_z_x[i];
// c->forbidden_y_x[i] |= c->proj_y_z[k];

