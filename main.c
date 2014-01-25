#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>

#define N_BITFIELD uint64_t
#define N_INDEX size_t

struct Config {
	char max_z;
	char k;
	char best_k;
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
	char cardinal_x[N];
	char heights[N][N];
	char best_heights[N][N];
	};

void backtrack(struct Config *, size_t, size_t);
void backtrack_next(struct Config *, size_t, size_t);
void backtrack_pillar(struct Config *, size_t, size_t, N_BITFIELD, N_BITFIELD);

void backtrack_pillar(struct Config *c,
		size_t i, size_t j, N_BITFIELD mask_x, N_BITFIELD mask_y) {
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
	c->cardinal_x[i]++;
#if R_OPTIM3
	/* Optimisation: only use heights in order */
	max_k = c->max_z;
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
#if R_OPTIM3
		if (k+1 == c->max_z && c->max_z < N) {
			c->max_z++;
			// Recursive call to backtrack
			backtrack_next(c, i, j);
			c->max_z--;
		} else
#endif
			backtrack_next(c, i, j);

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
	c->cardinal_x[i]--;
	backtrack_next(c, i, j);
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

void update_best(struct Config *c) {
	for (size_t i = 0; i < N; i++) {
		for (size_t j = 0; j < N; j++)
			c->best_heights[i][j] =
				c->heights[i][j];
	}
	c->best_k = c->k;
#if ROOKS_PRINT
	printf("----- BEST ! ----- %i\n", c->k);
	print_config(c);
#endif
}

void backtrack_next(struct Config *c, size_t i, size_t j) {
	/* Are we at the end ?*/
	if (i == 0 && j == 0) {
		if (c->k > c->best_k) {
			update_best(c);
		}
		return;
	}

#if R_OPTIM4
	/* Do we have a chance at beating the record ? */
	int max_slots = N*N;
	int max_available_slots = max_slots - (N-1-i)*N - (N-1-j);
	if (c->k + max_available_slots <= c->best_k) {
		return;
	}
#endif

#if R_OPTIM2
	/* Keep co-slices sorted */
	if (j < N - 1 && c->proj_x_y[j] > c->proj_x_y[j+1]) {
		return;
	}
#endif

#if R_OPTIM1
	// TODO: optimise by switching to the next slice
	if((i < N - 1 && c->cardinal_x[i] > c->cardinal_x[i+1])) {
		return;
	}
#endif

	/* Should we change slice ? */
	if (j == 0) {
		backtrack(c, i-1, N-1);
		return;
	}
	backtrack(c, i, j-1);
}

void backtrack(struct Config *c, N_INDEX i, N_INDEX j) {
	assert(i < N);
	assert(j < N);
	N_BITFIELD mask_x = 1 << i;
	N_BITFIELD mask_y = 1 << j;
	if ((c->forbidden_y_x[i] & mask_y)
			|| (c->forbidden_x_y[j] & mask_x))
		backtrack_next(c, i, j);
	else
		backtrack_pillar(c, i, j, mask_x, mask_y);
}

void * monitor(void *c) {
	for(;;) {
		sleep(1);
		print_config((struct Config *) c);
	}
}

int main(int argc, char* argv[])
{
#if ROOKS_MONITOR
	pthread_t t;
#endif
	struct Config c;

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
		c.cardinal_x[i] = 0;
	}
	c.k = 0;
	c.best_k = 0;
	c.max_z = 1;


#if ROOKS_MONITOR
	pthread_create(&t, NULL, monitor, &c);
#endif

	backtrack(&c, N-1, N-1);

	return 0;
}
