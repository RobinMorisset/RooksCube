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

#if R_OPTIM1
#define OPTIM1_CHECK(c, i, j) (c->cardinal_x[i] > c->cardinal_x[i+1])
#define OPTIM1(c, i, j) do { \
		if (OPTIM1_CHECK(c, i, j)) { \
			update_counter(1); \
			return; \
		} \
	} while (0)
#else
#define OPTIM1_CHECK(c, i, j) false
#define OPTIM1(c, i, j) do {} while (0)
#endif

#if R_OPTIM6
#define OPTIM6_CHECK(c, i, j) (c->cardinal_x[i] == c->cardinal_x[i+1] \
			&& c->proj_y_x[i] > c->proj_y_x[i+1])
#define OPTIM6(c, i, j) do { \
		if (OPTIM6_CHECK(c, i, j)) { \
			update_counter(6); \
			return; \
		} \
	} while (0)
#else
#define OPTIM6_CHECK(c, i, j) false
#define OPTIM6(c, i, j) do {} while (0)
#endif

#if R_OPTIM2
#define OPTIM2_CHECK(c, i, j) (c->proj_x_y[j] > c->proj_x_y[j+1])
#define OPTIM2(c, i, j) do { \
		/* Keep co-slices sorted */ \
		if (OPTIM2_CHECK(c, i, j)) { \
			update_counter(2); \
			return; \
		} \
	} while (0)
#else
#define OPTIM2_CHECK(c, i, j) false
#define OPTIM2(c, i, j) do {} while(0)
#endif

#if R_OPTIM4
#define OPTIM4() do { \
		/* Do we have a chance at beating the record ? */ \
		int max_available_slots = c->cardinal_x[i+1]*i + j; \
		if (c->card + max_available_slots <= c->best_card) { \
			update_counter(4); \
			return; \
		} \
	} while (0)
#define OPTIM4_INITIAL_LINE() do { \
		/* Do we have a chance at beating the record ? */ \
		if (N*(c->card + j) <= c->best_card) { \
			update_counter(4); \
			return; \
		} \
	} while(0)
#else
#define OPTIM4() do {} while(0)
#define OPTIM4_INITIAL_LINE() do {} while(0)
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
	N_INDEX cardinal_x[N];
	N_INDEX max_z;
	N_INDEX card;
	N_INDEX best_card;
	void print_config();
	void update_best();
} Config;

inline void __attribute__((always_inline))
	recurse_initial_line(Config *, N_INDEX, N_INDEX);
inline void __attribute__((always_inline)) recurse(Config *, N_INDEX, N_INDEX);

/* Template black magic, I don't know who designed the C++ templates but
 * they are insane */
template<bool on_initial_line, bool on_initial_column>
struct Worker final {
	static void backtrack(Config * c, N_INDEX, N_INDEX);
};

template<bool on_initial_line, bool on_initial_column, bool empty_slot>
struct Worker_next final {
	static void backtrack_next(Config * c, N_INDEX, N_INDEX);
};
template<bool on_initial_column>
struct Worker_next<true, on_initial_column, true> final {
	static void backtrack_next(Config * c, N_INDEX i, N_INDEX j) {
		OPTIM4_INITIAL_LINE();
		recurse_initial_line(c, i, j);
	}
};
template<bool on_initial_column>
struct Worker_next<true, on_initial_column, false> final {
	static void backtrack_next(Config * c, N_INDEX i, N_INDEX j) {
		recurse_initial_line(c, i, j);
	}
};
template<> struct Worker_next<false, true, true> final {
	static void backtrack_next(Config * c, N_INDEX i, N_INDEX j) {
		Worker<false, false>::backtrack(c, i, j-1);
	}
};
template<> struct Worker_next<false, true, false> final {
	static void backtrack_next(Config * c, N_INDEX i, N_INDEX j) {
		Worker<false, false>::backtrack(c, i, j-1);
	}
};
template<> struct Worker_next<false, false, true> final {
	static void backtrack_next(Config * c, N_INDEX i, N_INDEX j) {
		OPTIM4();
		recurse(c, i, j);
	}
};
template<> struct Worker_next<false, false, false> final {
	static void backtrack_next(Config * c, N_INDEX i, N_INDEX j) {
		OPTIM1(c, i, j);
		OPTIM6(c, i, j);
		OPTIM2(c, i, j);
		recurse(c, i, j);
	}
};

inline void __attribute__((always_inline))
		recurse_initial_line(Config * c, N_INDEX i, N_INDEX j) {
	/* Should we change slice ? */
	if (j == 0) {
		Worker<false, true>::backtrack(c, i-1, N-1);
	} else {
		Worker<true, false>::backtrack(c, i, j-1);
	}
}

inline void __attribute__((always_inline))
		recurse(Config * c, N_INDEX i, N_INDEX j) {
/* Should we change slice ? */
	if (j == 0) {
#if R_OPTIM5
		if (c->cardinal_x[i]+1 < c->cardinal_x[N-1]) {
			update_counter(5);
			return;
		}
#endif
#if R_MAX
		if ((c->cardinal_x[N-1]-1)*i + c->card > R_MAX) {
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
		Worker<false, true>::backtrack(c, i-1, N-1);
	} else {
		Worker<false, false>::backtrack(c, i, j-1);
	}
}

template<bool on_initial_line, bool on_initial_column>
void Worker<on_initial_line, on_initial_column>::backtrack(Config * c,
		N_INDEX i, N_INDEX j) {
	assert(i < N);
	assert(j < N);

	N_INDEX k, k_1 = 0, offset_k, max_k;
	N_BITFIELD mask_z, allowed_z, forbidden_z_mix;
	N_BITFIELD old_fzx, old_fzy, old_fyx, old_fyz, old_fxy, old_fxz;

	N_BITFIELD mask_x = 1 << i;
	N_BITFIELD mask_y = 1 << j;

	if ((c->forbidden_x[i][0] & mask_y)
			|| (c->forbidden_y[j][0] & mask_x))
		goto skip_slot_forbidden;

	c->proj_x_y[j] |= mask_x;
	if(OPTIM2_CHECK(c, i, j))
		goto skip_slot_optim2;

	c->cardinal_x[i]++;
	if(OPTIM1_CHECK(c, i, j)) {
		c->cardinal_x[i]--;
		c->proj_x_y[j] ^= mask_x;
		j = 0; // skips ahead to the end of the line
		goto skip_slot_forbidden;
	}

	c->proj_y_x[i] |= mask_y;
	// Save to restore later
	old_fyx = c->forbidden_x[i][0]; // TODO: save only once per slice
	old_fzx = c->forbidden_x[i][1];
	old_fxy = c->forbidden_y[j][0];
	old_fzy = c->forbidden_y[j][1];
	assert(i < N);
	assert(j < N);
	c->card++;

#if R_OPTIM3
	/* Optimisation: only use heights in order */
	// TODO: directly store a max_mask instead ?
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
		c->forbidden_x[i][0] = old_fyx | c->proj_y_z[k];
		c->forbidden_x[i][1] = old_fzx | c->proj_z_y[j];
		c->forbidden_y[j][0] = old_fxy | c->proj_x_z[k];
		c->forbidden_y[j][1] = old_fzy | c->proj_z_x[i];
		c->forbidden_z[k][0] = old_fxz | c->proj_x_y[j];
		c->forbidden_z[k][1] = old_fyz | c->proj_y_x[i];
#if R_OPTIM3
		if (c->max_z < N && k_1 == c->max_z) {
			c->max_z++;
			// Recursive call to backtrack
			Worker_next<on_initial_line, on_initial_column, false
				>::backtrack_next(c, i, j);
			c->max_z--;
			// TODO: we know this is the last iteration of the loop
		} else
#endif
			Worker_next<on_initial_line, on_initial_column, false
				>::backtrack_next(c, i, j);

		// Restore old values
		c->forbidden_x[i][0] = old_fyx;
		c->forbidden_x[i][1] = old_fzx;
		c->forbidden_y[j][0] = old_fxy;
		c->forbidden_y[j][1] = old_fzy;
		c->forbidden_z[k][0] = old_fxz;
		c->forbidden_z[k][1] = old_fyz;
		c->proj_z_x[i] ^= mask_z;
		c->proj_z_y[j] ^= mask_z;
		c->proj_y_z[k] ^= mask_y;
		c->proj_x_z[k] ^= mask_x;
	}

	// Finally try leaving the pillar empty
	c->heights[i][j] = 0;
	c->card--;
	c->proj_y_x[i] ^= mask_y;
	c->cardinal_x[i]--;
skip_slot_optim2:
	c->proj_x_y[j] ^= mask_x;
skip_slot_forbidden:
	Worker_next<on_initial_line, on_initial_column, true
		>::backtrack_next(c, i, j);
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
	printf("result: %zu | %zu\n\n", card, best_card);
}

void Config::update_best() {
	for (N_INDEX i = 0; i < N; i++) {
		for (N_INDEX j = 0; j < N; j++)
			best_heights[i][j] =
				heights[i][j];
	}
	best_card = card;
#if ROOKS_PRINT
	printf("----- BEST ! ----- %zu\n", card);
	print_config();
#endif
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
#ifndef R_MIN
	c.best_card = 0;
#else
	c.best_card = R_MIN - 1;
#endif
	c.max_z = 1;


#if ROOKS_MONITOR
	pthread_create(&t, NULL, monitor, &c);
#endif

	Worker<true, true>::backtrack(&c, N-1, N-1);

	print_counter();
	return 0;
}
