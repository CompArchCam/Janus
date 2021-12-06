CC = gcc

flagsSSE = -std=c11 -O3 -fivopts -flax-vector-conversions -funsafe-math-optimizations -Wall -Werror -fno-pie -no-pie 
flagsAVX = -std=c11 -O3 -fivopts -flax-vector-conversions -funsafe-math-optimizations -mavx -Wall -Werror -fno-pie -no-pie 
vecflags = -ftree-vectorizer-verbose=1
novecflags = -fno-tree-vectorize
libs = -lm
noopt = -O0

all : single double

single: runvec runnovec

double: runvec-d runnovec-d

runnovec : tscnovec.o dummy.o
	$(CC) $(noopt) dummy.o tscnovec.o -fno-pie -no-pie -o runnovec $(libs)

runvec : tscvec.o dummy.o
	$(CC) $(noopt) dummy.o tscvec.o -fno-pie -no-pie -o runvec $(libs)

tscvec.o : tsc.c
	rm -f report.lst
	$(CC) $(flagsSSE) $(vecflags) -c -o tscvec.o tsc.c  2> reportgcc.lst

tscnovec.o : tsc.c
	$(CC) $(flagsSSE) $(novecflags) -c -o tscnovec.o tsc.c

tsc.s : tsc.c dummy.o
	$(CC) $(flagsSSE) dummy.o tsc.c -S 

runnovec-d : tscnovec-d.o dummy-d.o
	$(CC) $(noopt) dummy-d.o tscnovec-d.o -fno-pie -no-pie -o runnovec-d $(libs)

runvec-d : tscvec-d.o dummy-d.o
	$(CC) $(noopt) dummy-d.o tscvec-d.o -fno-pie -no-pie -o runvec-d $(libs)

tscvec-d.o : tsc_double.c
	rm -f report.lst
	$(CC) $(flagsSSE) $(vecflags) -c -o tscvec-d.o tsc_double.c  2> reportgcc.lst

tscnovec-d.o : tsc_double.c
	$(CC) $(flagsSSE) $(novecflags) -c -o tscnovec-d.o tsc_double.c

tsc-d.s : tsc_double.c dummy-d.o
	$(CC) $(flagsSSE) dummy-d.o tsc_double.c -S 

dummy.o : dummy.c
	$(CC) -c dummy.c

dummy-d.o : dummy_double.c
	$(CC) -c -o dummy-d.o dummy_double.c

all-avx : single-avx double-avx

single-avx: runvec-avx runnovec-avx

double-avx: runvec-d-avx runnovec-d-avx

runnovec-avx : tscnovec-avx.o dummy.o
	$(CC) $(noopt) dummy.o tscnovec-avx.o -fno-pie -no-pie -o runnovec-avx $(libs)

runvec-avx : tscvec-avx.o dummy.o
	$(CC) $(noopt) dummy.o tscvec-avx.o -fno-pie -no-pie -o runvec-avx $(libs)

tscvec-avx.o : tsc.c
	rm -f report-avx.lst
	$(CC) $(flagsAVX) $(vecflags) -c -o tscvec-avx.o tsc.c  2> reportgcc-avx.lst

tscnovec-avx.o : tsc.c
	$(CC) $(flagsAVX) $(novecflags) -c -o tscnovec-avx.o tsc.c

tsc-avx.s : tsc.c dummy.o
	$(CC) $(flagsAVX) dummy.o tsc.c -S 

runnovec-d-avx : tscnovec-d-avx.o dummy-d.o
	$(CC) $(noopt) dummy-d.o tscnovec-d-avx.o -fno-pie -no-pie -o runnovec-d-avx $(libs)

runvec-d-avx : tscvec-d-avx.o dummy-d.o
	$(CC) $(noopt) dummy-d.o tscvec-d-avx.o -fno-pie -no-pie -o runvec-d-avx $(libs)

tscvec-d-avx.o : tsc_double.c
	rm -f report-avx.lst
	$(CC) $(flagsAVX) $(vecflags) -c -o tscvec-d-avx.o tsc_double.c  2> reportgcc-avx.lst

tscnovec-d-avx.o : tsc_double.c
	$(CC) $(flagsAVX) $(novecflags) -c -o tscnovec-d-avx.o tsc_double.c

tsc-d-avx.s : tsc_double.c dummy-d.o
	$(CC) $(flagsAVX) dummy-d.o tsc_double.c -S 

clean :
	rm -f *.o runnovec-d runvec-d *.lst *.s
	rm -f *.o runnovec runvec *.lst *.s
	rm -f *.o runnovec-d-avx runvec-d-avx *.lst *.s
	rm -f *.o runnovec-avx runvec-avx *.lst *.s


