#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>

#define N 5
#define N_BITFIELD uint64_t
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
	int best_k;
	char best_heights[N][N];
};

void backtrack(struct Config *, size_t, size_t);

void backtrack_pillar(struct Config *c,
		size_t i, size_t j, N_BITFIELD mask_x, N_BITFIELD mask_y,
		size_t i2, size_t j2) {
	size_t max_k;
	N_BITFIELD mask_z;
	N_BITFIELD old_fzx, old_fzy, old_fyx, old_fyz, old_fxy, old_fxz;

	N_BITFIELD forbidden_z = c->forbidden_z_x[i] | c->forbidden_z_y[j];

	// Save to restore later
	old_fzx = c->forbidden_z_x[i]; // TODO: save only once per slice
	old_fyx = c->forbidden_y_x[i];
	old_fzy = c->forbidden_z_y[j];
	old_fxy = c->forbidden_x_y[j];
	assert(i < N);
	assert(j < N);
	c->k++;
#if R_OPTIM1
	max_k = c->k < N ? c->k : N;
#else
	max_k = N;
#endif
	for (size_t k = 0; k < max_k; k++) {
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
		c->proj_x_y[j] |= mask_x;
		c->forbidden_z_x[i] |= c->proj_z_y[j];
		c->forbidden_z_y[j] |= c->proj_z_x[i];
		c->forbidden_y_x[i] |= c->proj_y_z[k];
		c->forbidden_y_z[k] |= c->proj_y_x[i];
		c->forbidden_x_y[j] |= c->proj_x_z[k];
		c->forbidden_x_z[k] |= c->proj_x_y[j];

		// Recursive call to backtrack
		backtrack(c, i2, j2);

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
	backtrack(c, i2, j2);
}

void print_config(struct Config * c) {
	for (size_t i = 0; i < N; i++) {
		for (size_t j = 0; j < N; j++) {
			if (c->heights[i][j])
				printf("%i ", c->heights[i][j]);
			else
				printf("* ");
		}
		printf(" -> %i\n", c->proj_y_x[i]);
	}
	printf("result: %i\n\n", c->k);
}

void backtrack(struct Config *c, size_t i, size_t j) {
	size_t i2, j2;
	N_BITFIELD mask_x, mask_y, old_hpyx;

#if R_OPTIM2
	/* Optimisation: keep the slices sorted */
	if (j == 0 && i >= 2 && c->proj_y_x[i-1] < c->proj_y_x[i-2]) {
		return;
	}
#endif

	/* If we are beyond the end, stop */
	if (i == N) {
		if (c->k > c->best_k) {
			for (size_t tmp_i = 0; tmp_i < N; tmp_i++) {
				for (size_t tmp_j = 0; tmp_j < N; tmp_j++)
					c->best_heights[tmp_i][tmp_j] = c->heights[tmp_i][tmp_j];
			}
			c->best_k = c->k;
#if ROOKS_PRINT
			printf("----- BEST ! ----- %i\n", c->k);
			print_config(c);
#endif
		}
		return;
	}

#if R_OPTIM3
	/* Optimisation: don't keep going if there is no chance of beating the record */
	int max_slots = N*N;
	int max_available_slots = max_slots - i*N - j;
	if (c->k + max_available_slots <= c->best_k)
		return;
#endif

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
		backtrack(c, i2, j2);
	else
		backtrack_pillar(c, i, j, mask_x, mask_y, i2, j2);

}

void * monitor(void *c) {
	for(;;) {
		sleep(1);
		print_config((struct Config *) c);
	}
}

int main(int argc, char* argv[])
{
	pthread_t t;
	struct Config c;
	struct Config b;

	/* Initialisation */
	for (size_t i = 0; i < N; i++) {
		for (size_t j = 0; j < N; j++) {
			c.heights[i][j] = 0;
			c.best_heights[i][j] = 0;
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
	c.best_k = 0;


#if ROOKS_MONITOR
	pthread_create(&t, NULL, monitor, &c);
#endif

	backtrack(&c, 0, 0);

	return 0;
}
