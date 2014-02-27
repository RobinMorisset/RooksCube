#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>

#define N_BITFIELD uint32_t
#define N_BITFIELD2 uint64_t
#define N_INDEX size_t
#define FFS_BITFIELD(x) __builtin_ffs(x)

#if R_COUNTER
int counter[7];
inline void update_counter(size_t i) {counter[i]++;}
inline void print_counter(void) {
	for (int i = 0; i < 7; i++) {
		printf("Number of configurations stopped by optimisation %i: %i\n"
				, i, counter[i]);
	}
}
#else
inline void update_counter(size_t i) {}
inline void print_counter(void) {}
#endif

typedef struct Config final {
	// the index corresponds to x, the bitfield to a 'pillar' (z)
	N_BITFIELD forbidden_x[N][2];
	N_BITFIELD forbidden_y[N][2];
	N_BITFIELD forbidden_z[N][2];
	N_BITFIELD proj_z_x[N];
	N_BITFIELD proj_z_y[N];
	N_BITFIELD proj_y_z[N];
	N_BITFIELD proj_y_x[N];
	N_BITFIELD proj_x_y[N+1];
	N_BITFIELD proj_x_z[N];
	char heights[N][N];
	char best_heights[N][N];
	char cardinal_x[N];
	char max_z;
	char card;
	char best_card;
	void print_config();
	void update_best();
} Config;

/* Template black magic, I don't know who designed the C++ templates but
 * they are insane */
template<bool on_initial_line>
struct Worker final {
	static void backtrack(Config * c, N_INDEX, N_INDEX);
	static void backtrack_pillar(Config * c, N_INDEX, N_INDEX,
			N_BITFIELD, N_BITFIELD);
};
template<bool on_initial_line, bool empty_slot>
struct Worker_next final {
	static void backtrack_next(Config * c, N_INDEX, N_INDEX);
};
template<>
struct Worker_next<true, true> final {
	static void backtrack_next(Config * c, N_INDEX, N_INDEX);
};
template<>
struct Worker_next<true, false> final {
	static void backtrack_next(Config * c, N_INDEX, N_INDEX);
};
template<>
struct Worker_next<false, true> final {
	static void backtrack_next(Config * c, N_INDEX, N_INDEX);
};
template<>
struct Worker_next<false, false> final {
	static void backtrack_next(Config * c, N_INDEX, N_INDEX);
};

template<bool on_initial_line>
void Worker<on_initial_line>::backtrack_pillar(Config * c,
		N_INDEX i, N_INDEX j, N_BITFIELD mask_x, N_BITFIELD mask_y) {
	N_INDEX k, k_1 = 0, offset_k, max_k;
	N_BITFIELD mask_z, allowed_z, forbidden_z_mix;
	N_BITFIELD old_fzx, old_fzy, old_fyx, old_fyz, old_fxy, old_fxz;
	// N_BITFIELD2 old_fx;

	// Save to restore later
	// old_fx = (N_BITFIELD2) forbidden_x[i];
	old_fyx = c->forbidden_x[i][0]; // TODO: save only once per slice
	old_fzx = c->forbidden_x[i][1];
	old_fxy = c->forbidden_y[j][0];
	old_fzy = c->forbidden_y[j][1];
	assert(i < N);
	assert(j < N);
	c->card++;
	c->cardinal_x[i]++;
	c->proj_y_x[i] |= mask_y;
	c->proj_x_y[j] |= mask_x;

#if R_OPTIM3
	/* Optimisation: only use heights in order */
	max_k = c->max_z;
#else
	max_k = N;
#endif
	forbidden_z_mix = c->forbidden_x[i][1] | c->forbidden_y[j][1];
	allowed_z = (~forbidden_z_mix) & ((1 << max_k) - 1); // TODO: optimize
	while ((offset_k = FFS_BITFIELD(allowed_z))) {
		k_1 += offset_k;
		allowed_z = allowed_z >> offset_k;
		k = k_1 - 1;
		if ((mask_x & c->forbidden_z[k][0])
				|| (mask_y & c->forbidden_z[k][1]))
			continue;
		mask_z = 1 << k;

		// Save to restore later
		old_fyz = c->forbidden_z[k][1];
		old_fxz = c->forbidden_z[k][0];

		// Add the rook
		c->heights[i][j] = k_1; // because 0 is no rook
		c->proj_z_x[i] |= mask_z;
		c->proj_z_y[j] |= mask_z;
		c->proj_y_z[k] |= mask_y;
		c->proj_x_z[k] |= mask_x;
		c->forbidden_x[i][1] |= c->proj_z_y[j];
		c->forbidden_y[j][1] |= c->proj_z_x[i];
		c->forbidden_x[i][0] |= c->proj_y_z[k];
		c->forbidden_z[k][1] |= c->proj_y_x[i];
		c->forbidden_y[j][0] |= c->proj_x_z[k];
		c->forbidden_z[k][0] |= c->proj_x_y[j];
#if R_OPTIM3
		if (c->max_z < N && k_1 == c->max_z) {
			c->max_z++;
			// Recursive call to backtrack
			Worker_next<on_initial_line, false>::backtrack_next(c, i, j);
			c->max_z--;
		} else
#endif
			Worker_next<on_initial_line, false>::backtrack_next(c, i, j);

		// Restore old values
		c->forbidden_x[i][1] = old_fzx;
		c->forbidden_y[j][1] = old_fzy;
		c->forbidden_x[i][0] = old_fyx;
		c->forbidden_z[k][1] = old_fyz;
		c->forbidden_y[j][0] = old_fxy;
		c->forbidden_z[k][0] = old_fxz;
		// forbidden_x[i] = old_fx;
		c->proj_z_x[i] ^= mask_z;
		c->proj_z_y[j] ^= mask_z;
		c->proj_y_z[k] ^= mask_y;
		c->proj_x_z[k] ^= mask_x;
	}

	// Finally try leaving the pillar empty
	c->heights[i][j] = 0;
	c->proj_y_x[i] ^= mask_y;
	c->proj_x_y[j] ^= mask_x;
	c->card--;
	c->cardinal_x[i]--;
	Worker_next<on_initial_line, true>::backtrack_next(c, i, j);
}

void print_rook(uint8_t h) {
	if (h == 0)
		printf("* ");
	else if (h < 10)
		printf("%i ", h);
	else
		printf("%c ", 'A' + h - 10);
}

void Config::print_config() {
	for (N_INDEX i = 0; i < N; i++) {
		for (N_INDEX j = 0; j < N; j++) {
			print_rook(heights[i][j]);
		}
		printf("\n");
	}
	printf("result: %i | %i\n\n", card, best_card);
}

void Config::update_best() {
	for (N_INDEX i = 0; i < N; i++) {
		for (N_INDEX j = 0; j < N; j++)
			best_heights[i][j] =
				heights[i][j];
	}
	best_card = card;
#if ROOKS_PRINT
	printf("----- BEST ! ----- %i\n", card);
	print_config();
#endif
}

void Worker_next<false, false>::backtrack_next(Config * c,
		N_INDEX i, N_INDEX j) {
#if R_OPTIM1
	if (c->cardinal_x[i] > c->cardinal_x[i+1]) {
		update_counter(1);
		return;
	}
#endif
#if R_OPTIM6
	if (c->cardinal_x[i] == c->cardinal_x[i+1]
			&& c->proj_y_x[i] > c->proj_y_x[i+1]) {
		update_counter(6);
		return;
	}
#endif
#if R_OPTIM2
	/* Keep co-slices sorted */
	if (c->proj_x_y[j] > c->proj_x_y[j+1]) {
		update_counter(2);
		return;
	}
#endif

/* Should we change slice ? */
	if (j == 0) {
#if R_OPTIM5
		if (c->cardinal_x[i]+1 < c->cardinal_x[N-1]) {
			update_counter(5);
			return;
		}
#endif
		/* Are we at the end ? */
		if (i == 0) {
			if (c->card > c->best_card) {
				c->update_best();
			}
			update_counter(0);
			return;
		}
		Worker<false>::backtrack(c, i-1, N-1);
	} else {
		Worker<false>::backtrack(c, i, j-1);
	}
}

// TODO: check validity of adding rooks before adding them
void Worker_next<false, true>::backtrack_next(Config * c,
		N_INDEX i, N_INDEX j) {
#if R_OPTIM4
	/* Do we have a chance at beating the record ? */
	int max_available_slots = c->cardinal_x[i+1]*i + j;
	if (c->card + max_available_slots <= c->best_card) {
		update_counter(4);
		return;
	}
#endif
	/* Should we change slice ? */
	if (j == 0) {
#if R_OPTIM5
		if (c->cardinal_x[i]+1 < c->cardinal_x[N-1]) {
			update_counter(5);
			return;
		}
#endif
		/* Are we at the end ? */
		if (i == 0) {
			if (c->card > c->best_card) {
				c->update_best();
			}
			update_counter(0);
			return;
		}
		Worker<false>::backtrack(c, i-1, N-1);
	} else {
		Worker<false>::backtrack(c, i, j-1);
	}
}

void Worker_next<true, true>::backtrack_next(Config * c,
		N_INDEX i, N_INDEX j) {
#if R_OPTIM4
	/* Do we have a chance at beating the record ? */
	if (N*(c->card + j) <= c->best_card) {
		update_counter(4);
		return;
	}
#endif

/* Should we change slice ? */
	if (j == 0) {
		Worker<false>::backtrack(c, i-1, N-1);
	} else {
		Worker<true>::backtrack(c, i, j-1);
	}
}

void Worker_next<true, false>::backtrack_next(Config * c,
		N_INDEX i, N_INDEX j) {
#if R_OPTIM2
	/* Keep co-slices sorted */
	if (c->proj_x_y[j] > c->proj_x_y[j+1]) {
		update_counter(2);
		return;
	}
#endif

	/* Should we change slice ? */
	if (j == 0) {
		Worker<false>::backtrack(c, i-1, N-1);
	} else {
		Worker<true>::backtrack(c, i, j-1);
	}
}

template<bool on_initial_line>
void Worker<on_initial_line>::backtrack(Config * c,
		N_INDEX i, N_INDEX j) {
	assert(i < N);
	assert(j < N);
	N_BITFIELD mask_x = 1 << i;
	N_BITFIELD mask_y = 1 << j;

	if ((c->forbidden_x[i][0] & mask_y)
			|| (c->forbidden_y[j][0] & mask_x))
		Worker_next<on_initial_line, true>::backtrack_next(c, i, j);
	else
		backtrack_pillar(c, i, j, mask_x, mask_y);
}

void * monitor(void *c) {
	for(;;) {
		sleep(1);
		((struct Config *) c)->print_config();
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
		for (size_t j = 0; j < 2; j++) {
			c.forbidden_x[i][j] = 0;
			c.forbidden_y[i][j] = 0;
			c.forbidden_z[i][j] = 0;
		}
		c.proj_z_x[i] = 0;
		c.proj_z_y[i] = 0;
		c.proj_y_z[i] = 0;
		c.proj_y_x[i] = 0;
		c.proj_x_y[i] = 0;
		c.proj_x_z[i] = 0;
		c.cardinal_x[i] = 0;
	}
	c.proj_x_y[N] = (N_BITFIELD) (-1);
	c.card = 0;
#ifndef R_BEST
	c.best_card = 0;
#else
	c.best_card = R_BEST;
#endif
	c.max_z = 1;


#if ROOKS_MONITOR
	pthread_create(&t, NULL, monitor, &c);
#endif

	Worker<true>::backtrack(&c, N-1, N-1);

	print_counter();
	return 0;
}
