#ifndef _X86_H
#define _X86_H

#include <stdint.h>

static inline int atomic_ctz_64(uint64_t x)
{
	__asm__( "bsf %1,%0" : "=r"(x) : "r"(x) );
	return x;
}

static inline int atomic_ctz_l(unsigned long x)
{
	__asm__( "bsf %1,%0" : "=r"(x) : "r"(x) );
	return x;
}

static inline void atomic_and_64(volatile uint64_t *p, uint64_t v)
{
	__asm__( "lock ; and %1, %0"
			 : "=m"(*p) : "r"(v) : "memory" );
}

static inline void atomic_or_64(volatile uint64_t *p, uint64_t v)
{
	__asm__( "lock ; or %1, %0"
			 : "=m"(*p) : "r"(v) : "memory" );
}

static inline void atomic_or_l(volatile void *p, long v)
{
	__asm__( "lock ; or %1, %0"
		: "=m"(*(long *)p) : "r"(v) : "memory" );
}

static inline void *atomic_cas_p(volatile void *p, void *t, void *s)
{
	__asm__( "lock ; cmpxchg %3, %1"
		: "=a"(t), "=m"(*(long *)p) : "a"(t), "r"(s) : "memory" );
	return t;
}

static inline int atomic_cas(volatile int *p, int t, int s)
{
	__asm__( "lock ; cmpxchg %3, %1"
		: "=a"(t), "=m"(*p) : "a"(t), "r"(s) : "memory" );
	return t;
}

static inline void atomic_or(volatile void *p, int v)
{
	__asm__( "lock ; or %1, %0"
		: "=m"(*(int *)p) : "r"(v) : "memory" );
}

static inline void atomic_and(volatile void *p, int v)
{
	__asm__( "lock ; and %1, %0"
		: "=m"(*(int *)p) : "r"(v) : "memory" );
}

static inline int atomic_swap(volatile int *x, int v)
{
	__asm__( "xchg %0, %1" : "=r"(v), "=m"(*x) : "0"(v) : "memory" );
	return v;
}

static inline int atomic_fetch_add(volatile int *x, int v)
{
	__asm__( "lock ; xadd %0, %1" : "=r"(v), "=m"(*x) : "0"(v) : "memory" );
	return v;
}

static inline int atomic_fetch_and_add_64(volatile uint64_t *x, int v)
{
	__asm__( "lock ; xadd %0, %1" : "=r"(v), "=m"(*x) : "0"(v) : "memory" );
	return v;
}

static inline void atomic_inc(volatile int *x)
{
	__asm__( "lock ; incl %0" : "=m"(*x) : "m"(*x) : "memory" );
}

static inline void atomic_inc_64(volatile uint64_t *x)
{
	__asm__( "lock ; incl %0" : "=m"(*x) : "m"(*x) : "memory" );
}

static inline void atomic_dec(volatile int *x)
{
	__asm__( "lock ; decl %0" : "=m"(*x) : "m"(*x) : "memory" );
}

static inline void atomic_store(volatile int *p, int x)
{
	__asm__( "mov %1, %0" : "=m"(*p) : "r"(x) : "memory" );
}

static inline void atomic_spin()
{
	__asm__ __volatile__( "pause" : : : "memory" );
}

static inline void memory_barrier()
{
	__asm__ __volatile__( "" : : : "memory" );
}

static inline void atomic_crash()
{
	__asm__ __volatile__( "hlt" : : : "memory" );
}



#endif
