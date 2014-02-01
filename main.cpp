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
int counter[7];
inline void update_counter(void) {counter[0]++;}
inline void update_counter_1(void) {counter[1]++;}
inline void update_counter_2(void) {counter[2]++;}
inline void update_counter_3(void) {counter[3]++;}
inline void update_counter_4(void) {counter[4]++;}
inline void update_counter_5(void) {counter[5]++;}
inline void update_counter_6(void) {counter[6]++;}
inline void print_counter(void) {
	for (int i = 0; i < 7; i++) {
		printf("Number of configurations stopped by optimisation %i: %i\n"
				, i, counter[i]);
	}
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

struct Config final {
	char max_z;
	char card;
	char best_card;
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
	void backtrack(N_INDEX, N_INDEX);
	void backtrack_next(N_INDEX, N_INDEX);
	void backtrack_pillar(N_INDEX, N_INDEX, N_BITFIELD, N_BITFIELD);
	void print_config();
	void update_best();
};


void Config::backtrack_pillar(
		N_INDEX i, N_INDEX j, N_BITFIELD mask_x, N_BITFIELD mask_y) {
	N_INDEX k, k_1 = 0, offset_k, max_k;
	N_BITFIELD mask_z, allowed_z, forbidden_z;
	N_BITFIELD old_fzx, old_fzy, old_fyx, old_fyz, old_fxy, old_fxz;

	// Save to restore later
	old_fzx = forbidden_z_x[i]; // TODO: save only once per slice
	old_fyx = forbidden_y_x[i];
	old_fzy = forbidden_z_y[j];
	old_fxy = forbidden_x_y[j];
	assert(i < N);
	assert(j < N);
	card++;
	cardinal_x[i]++;

#if R_OPTIM3
	/* Optimisation: only use heights in order */
	max_k = max_z;
#else
	max_k = N;
#endif
	forbidden_z = forbidden_z_x[i] | forbidden_z_y[j];
	allowed_z = (~forbidden_z) & ((1 << max_k) - 1); // TODO: optimize
	while ((offset_k = FFS_BITFIELD(allowed_z))) {
		k_1 += offset_k;
		allowed_z = allowed_z >> offset_k;
		k = k_1 - 1;
		if ((mask_x & forbidden_x_z[k])
				|| (mask_y & forbidden_y_z[k]))
			continue;
		mask_z = 1 << k;

		// Save to restore later
		old_fyz = forbidden_y_z[k];
		old_fxz = forbidden_x_z[k];

		// Add the rook
		heights[i][j] = k_1; // because 0 is no rook
		proj_z_x[i] |= mask_z;
		proj_z_y[j] |= mask_z;
		proj_y_z[k] |= mask_y;
		proj_y_x[i] |= mask_y; // Could be done only once per loop
		     		  // but is weirdly slower
		proj_x_z[k] |= mask_x;
		proj_x_y[j] |= mask_x;
		forbidden_z_x[i] |= proj_z_y[j];
		forbidden_z_y[j] |= proj_z_x[i];
		forbidden_y_x[i] |= proj_y_z[k];
		forbidden_y_z[k] |= proj_y_x[i];
		forbidden_x_y[j] |= proj_x_z[k];
		forbidden_x_z[k] |= proj_x_y[j];
#if R_OPTIM3
		if (max_z < N && k_1 == max_z) {
			max_z++;
			// Recursive call to backtrack
			backtrack_next(i, j);
			max_z--;
		} else
#endif
			backtrack_next(i, j);

		// Restore old values
		forbidden_z_x[i] = old_fzx;
		forbidden_z_y[j] = old_fzy;
		forbidden_y_x[i] = old_fyx;
		forbidden_y_z[k] = old_fyz;
		forbidden_x_y[j] = old_fxy;
		forbidden_x_z[k] = old_fxz;
		proj_z_x[i] ^= mask_z;
		proj_z_y[j] ^= mask_z;
		proj_y_z[k] ^= mask_y;
		proj_y_x[i] ^= mask_y;
		proj_x_z[k] ^= mask_x;
		proj_x_y[j] ^= mask_x;
	}

	// Finally try leaving the pillar empty
	heights[i][j] = 0;
	card--;
	cardinal_x[i]--;
	backtrack_next(i, j);
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

// TODO: check validity of adding rooks before adding them
void Config::backtrack_next(N_INDEX i, N_INDEX j) {
#if R_OPTIM1
	if(i < N - 1) {
		if (cardinal_x[i] > cardinal_x[i+1]) {
			update_counter_1();
			return;
		}
#if R_OPTIM6
		if (cardinal_x[i] == cardinal_x[i+1]
				&& proj_y_x[i] > proj_y_x[i+1]) {
			update_counter_6();
			return;
		}
#endif
#if R_OPTIM4
		/* Do we have a chance at beating the record ? */
		int max_available_slots = cardinal_x[i+1]*i + j;
		if (card + max_available_slots <= best_card) {
			update_counter_4();
			return;
		}
#endif
#if R_OPTIM5
		if (j == 0 && cardinal_x[i] < (cardinal_x[N-1] - 1)) {
			update_counter_5();
			return;
		}
#endif
	}
#endif

#if R_OPTIM2
	/* Keep co-slices sorted */
	if (j < N - 1 && proj_x_y[j] > proj_x_y[j+1]) {
		update_counter_2();
		return;
	}
#endif

	/* Are we at the end ? */
	if (i == 0 && j == 0) {
		if (card > best_card) {
			update_best();
		}
		update_counter();
		return;
	}

	/* Should we change slice ? */
	if (j == 0) {
		backtrack(i-1, N-1);
	} else {
		backtrack(i, j-1);
	}
}

void Config::backtrack(N_INDEX i, N_INDEX j) {
	assert(i < N);
	assert(j < N);
	N_BITFIELD mask_x = 1 << i;
	N_BITFIELD mask_y = 1 << j;
	if ((forbidden_y_x[i] & mask_y)
			|| (forbidden_x_y[j] & mask_x))
		backtrack_next(i, j);
	else
		backtrack_pillar(i, j, mask_x, mask_y);
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

	c.backtrack(N-1, N-1);

	print_counter();
	return 0;
}
