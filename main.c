#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>

#define N_BITFIELD uint32_t
#define N_INDEX size_t
#define FFS_BITFIELD(x) __builtin_ffs(x)

#if R_COUNTER
int counter = 0;
int counter_1 = 0;
int counter_2 = 0;
int counter_4 = 0;
int counter_5 = 0;
int counter_6 = 0;

inline void update_counter(void) {
	counter++;
}

inline void update_counter_1(void) {
	counter++;
	counter_1++;
}

inline void update_counter_2(void) {
	counter++;
	counter_2++;
}

inline void update_counter_4(void) {
	counter++;
	counter_4++;
}

inline void update_counter_5(void) {
	counter++;
	counter_5++;
}

inline void update_counter_5(void) {
	counter++;
	counter_6++;
}

inline void print_counter(void) {
	printf("Number of configurations explored: %i\n", counter);
	printf("Number of configurations stopped by optimisation 1: %i\n",
			counter_1);
	printf("Number of configurations stopped by optimisation 2: %i\n",
			counter_2);
	printf("Number of configurations stopped by optimisation 4: %i\n",
			counter_4);
	printf("Number of configurations stopped by optimisation 5: %i\n",
			counter_5);
}
#else
inline void update_counter(void) {}
inline void update_counter_1(void){}
inline void update_counter_2(void){}
inline void update_counter_4(void){}
inline void update_counter_5(void){}
inline void update_counter_6(void){}
inline void print_counter(void) {}
#endif

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

void backtrack(struct Config *, N_INDEX, N_INDEX);
void backtrack_next(struct Config *, N_INDEX, N_INDEX);
void backtrack_pillar(struct Config *, N_INDEX, N_INDEX, N_BITFIELD, N_BITFIELD);

void backtrack_pillar(struct Config *c,
		N_INDEX i, N_INDEX j, N_BITFIELD mask_x, N_BITFIELD mask_y) {
	N_INDEX k, k_1 = 0, offset_k, max_k;
	N_BITFIELD mask_z, allowed_z, forbidden_z;
	N_BITFIELD old_fzx, old_fzy, old_fyx, old_fyz, old_fxy, old_fxz;

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
	forbidden_z = c->forbidden_z_x[i] | c->forbidden_z_y[j];
	allowed_z = (~forbidden_z) & ((1 << max_k) - 1);
	while ((offset_k = FFS_BITFIELD(allowed_z))) {
		k_1 += offset_k;
		allowed_z = allowed_z >> offset_k;
		k = k_1 - 1;
		if ((mask_x & c->forbidden_x_z[k])
				|| (mask_y & c->forbidden_y_z[k]))
			continue;
		mask_z = 1 << k;

		// Save to restore later
		old_fyz = c->forbidden_y_z[k];
		old_fxz = c->forbidden_x_z[k];

		// Add the rook
		c->heights[i][j] = k_1; // because 0 is no rook
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
		if (c->max_z < N && k_1 == c->max_z) {
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

void print_rook(uint8_t h) {
	if (h == 0)
		printf("* ");
	else if (h < 10)
		printf("%i ", h);
	else
		printf("%c ", 'A' + h - 10);
}

void print_config(struct Config * c) {
	for (N_INDEX i = 0; i < N; i++) {
		for (N_INDEX j = 0; j < N; j++) {
			print_rook(c->heights[i][j]);
		}
		printf("\n");
	}
	printf("result: %i | %i\n\n", c->k, c->best_k);
}

void update_best(struct Config *c) {
	for (N_INDEX i = 0; i < N; i++) {
		for (N_INDEX j = 0; j < N; j++)
			c->best_heights[i][j] =
				c->heights[i][j];
	}
	c->best_k = c->k;
#if ROOKS_PRINT
	printf("----- BEST ! ----- %i\n", c->k);
	print_config(c);
#endif
}

// TODO: check validity of adding rooks before adding them
void backtrack_next(struct Config *c, N_INDEX i, N_INDEX j) {
#if R_OPTIM1
	// TODO: optimise by switching to the next slice
	if(i < N - 1) {
		if (c->cardinal_x[i] > c->cardinal_x[i+1]) {
			update_counter_1();
			return;
		}
#if R_OPTIM6
		if (c->cardinal_x[i] == c->cardinal_x[i+1]
				&& c->proj_y_x[i] > c->proj_y_x[i+1]) {
			update_counter_6();
			return;
		}
#endif
#if R_OPTIM4
		/* Do we have a chance at beating the record ? */
		int max_available_slots = c->cardinal_x[i+1]*i + j;
		if (c->k + max_available_slots <= c->best_k) {
			update_counter_4();
			return;
		}
#endif
#if R_OPTIM5
		if (j == 0 && c->cardinal_x[i] < (c->cardinal_x[N-1] - 1)) {
			update_counter_5();
			return;
		}
#endif
	}
#endif

#if R_OPTIM2
	/* Keep co-slices sorted */
	if (j < N - 1 && c->proj_x_y[j] > c->proj_x_y[j+1]) {
		update_counter_2();
		return;
	}
#endif

	/* Are we at the end ? */
	if (i == 0 && j == 0) {
		if (c->k > c->best_k) {
			update_best(c);
		}
		update_counter();
		return;
	}

	/* Should we change slice ? */
	if (j == 0) {
		backtrack(c, i-1, N-1);
	} else {
		backtrack(c, i, j-1);
	}
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
	for (N_INDEX i = 0; i < N; i++) {
		for (N_INDEX j = 0; j < N; j++) {
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
#ifndef R_BEST
	c.best_k = 0;
#else
	c.best_k = R_BEST;
#endif
	c.max_z = 1;


#if ROOKS_MONITOR
	pthread_create(&t, NULL, monitor, &c);
#endif

	backtrack(&c, N-1, N-1);

	print_counter();
	return 0;
}
