#ifndef _X86_H
#define _X86_H

#include <stdint.h>

static inline int atomic_ctz_64(uint64_t x)
{
}

static inline int atomic_ctz_l(unsigned long x)
{
}

static inline void atomic_and_64(volatile uint64_t *p, uint64_t v)
{
}

static inline void atomic_or_64(volatile uint64_t *p, uint64_t v)
{
}

static inline void atomic_or_l(volatile void *p, long v)
{
}

static inline void *atomic_cas_p(volatile void *p, void *t, void *s)
{
}

static inline int atomic_cas(volatile int *p, int t, int s)
{
}

static inline void atomic_or(volatile void *p, int v)
{
}

static inline void atomic_and(volatile void *p, int v)
{
}

static inline int atomic_swap(volatile int *x, int v)
{
}

static inline int atomic_fetch_add(volatile int *x, int v)
{
}

static inline int atomic_fetch_and_add_64(volatile uint64_t *x, int v)
{
}

static inline void atomic_inc(volatile int *x)
{
}

static inline void atomic_inc_64(volatile uint64_t *x)
{
}

static inline void atomic_dec(volatile int *x)
{
}

static inline void atomic_store(volatile int *p, int x)
{
}

static inline void atomic_spin()
{
}

static inline void memory_barrier()
{
}

static inline void atomic_crash()
{
}

#endif
