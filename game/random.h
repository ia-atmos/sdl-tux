/* The random numbers of the original (`Random`, src/math/random.cpp of
 * SuperTux v0.7.0): a std::mt19937, and the distributions of libstdc++
 * (GCC 13), which the official game uses:
 *
 *  - uniform_real_distribution<float>(a, b): generate_canonical<float, 24>,
 *    one draw of 32 bits divided by 2^32 in float (nextafter(1, 0) if it
 *    rounds to 1), then `c * (b - a) + a`, in float;
 *  - uniform_int_distribution<int>(a, b): Lemire's nearly divisionless
 *    method over 64 bits, for a generator of exactly 32 bits.
 *
 * `Random::seed(v)` takes the time for v <= 0, as the original does for
 * graphicsRandom (`graphicsRandom.seed(0)`). The names have the prefix
 * `tr_`. */

#ifndef TR_RANDOM_H
#define TR_RANDOM_H

#include <stdint.h>
#include <math.h>
#include <time.h>

#define TR_N 624
#define TR_M 397

typedef struct TrRandom {
    uint32_t mt[TR_N];
    int      i;
} TrRandom;

/* std::mt19937::seed */
static void tr_seed_raw (TrRandom* r, uint32_t v) {
    r->mt[0] = v;
    for (int i=1; i<TR_N; i++) {
        r->mt[i] = 1812433253u * (r->mt[i-1] ^ (r->mt[i-1] >> 30)) + (uint32_t) i;
    }
    r->i = TR_N;
}

/* Random::seed */
static void tr_seed (TrRandom* r, int v) {
    if (v <= 0) {
        v = (int) time(NULL);
    }
    tr_seed_raw(r, (uint32_t) v);
}

/* std::mt19937::operator() */
static uint32_t tr_next (TrRandom* r) {
    if (r->i >= TR_N) {
        for (int k=0; k<TR_N; k++) {
            uint32_t y = (r->mt[k] & 0x80000000u) | (r->mt[(k+1) % TR_N] & 0x7fffffffu);
            r->mt[k] = r->mt[(k+TR_M) % TR_N] ^ (y >> 1) ^ ((y & 1u) ? 0x9908b0dfu : 0u);
        }
        r->i = 0;
    }
    uint32_t y = r->mt[r->i++];
    y ^= (y >> 11);
    y ^= (y << 7)  & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= (y >> 18);
    return y;
}

/* Random::randf(u, v), and randf(v) as (0, v) */
static float tr_randf (TrRandom* r, float u, float v) {
    float sum = (float) tr_next(r);
    float tmp = 4294967296.0f;
    float c = sum / tmp;
    if (c >= 1.0f) {
        c = nextafterf(1.0f, 0.0f);
    }
    return (c * (v - u)) + u;
}

/* Random::rand(u, v): an integer in [u, v) */
static int tr_rand (TrRandom* r, int u, int v) {
    uint32_t range = (uint32_t) (v - 1) - (uint32_t) u + 1u;     /* __uerange */
    uint64_t product = (uint64_t) tr_next(r) * (uint64_t) range;
    uint32_t low = (uint32_t) product;
    if (low < range) {
        uint32_t threshold = (uint32_t) (-range) % range;
        while (low < threshold) {
            product = (uint64_t) tr_next(r) * (uint64_t) range;
            low = (uint32_t) product;
        }
    }
    return u + (int) (uint32_t) (product >> 32);
}

#endif
