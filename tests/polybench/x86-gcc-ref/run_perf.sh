#!/bin/bash
THREAD=4

echo "2mm"
../../../bin/analyze -p 2mm
time (for i in {1..5}; do ./2mm 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD 2mm; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "3mm"
../../../bin/analyze -p 3mm
time (for i in {1..5}; do ./3mm 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD 3mm; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "adi"
../../../bin/analyze -p adi
time (for i in {1..5}; do ./adi 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD adi; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "atax"
../../../bin/analyze -p atax
time (for i in {1..5}; do ./atax 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD atax; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "bicg"
../../../bin/analyze -p bicg
time (for i in {1..5}; do ./bicg 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD bicg; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "cholesky"
../../../bin/analyze -p cholesky
time (for i in {1..5}; do ./cholesky 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD cholesky; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "correlation"
../../../bin/analyze -p correlation
time (for i in {1..5}; do ./correlation 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD correlation; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "covariance"
../../../bin/analyze -p covariance
time (for i in {1..5}; do ./covariance 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD covariance; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "deriche"
../../../bin/analyze -p deriche
time (for i in {1..5}; do ./deriche 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD deriche; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "doitgen"
../../../bin/analyze -p doitgen
time (for i in {1..5}; do ./doitgen 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD doitgen; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "durbin"
../../../bin/analyze -p durbin
time (for i in {1..5}; do ./durbin 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD durbin; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "fdtd-2d"
../../../bin/analyze -p fdtd-2d
time (for i in {1..5}; do ./fdtd-2d 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD fdtd-2d; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "floyd-warshall"
../../../bin/analyze -p floyd-warshall
time (for i in {1..5}; do ./floyd-warshall 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD floyd-warshall; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "gemm"
../../../bin/analyze -p gemm
time (for i in {1..5}; do ./gemm 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD gemm; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "gemver"
../../../bin/analyze -p gemver
time (for i in {1..5}; do ./gemver 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD gemver; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "gesummv"
../../../bin/analyze -p gesummv
time (for i in {1..5}; do ./gesummv 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD gesummv; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "gramschmidt"
../../../bin/analyze -p gramschmidt
time (for i in {1..5}; do ./gramschmidt 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD gramschmidt; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "heat-3d"
../../../bin/analyze -p heat-3d
time (for i in {1..5}; do ./heat-3d 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD heat-3d; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "jacobi-1d"
../../../bin/analyze -p jacobi-1d
time (for i in {1..5}; do ./jacobi-1d 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD jacobi-1d; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "jacobi-2d"
../../../bin/analyze -p jacobi-2d
time (for i in {1..5}; do ./jacobi-2d 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD jacobi-2d; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "lu"
../../../bin/analyze -p lu
time (for i in {1..5}; do ./lu 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD lu; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "ludcmp"
../../../bin/analyze -p ludcmp
time (for i in {1..5}; do ./ludcmp 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD ludcmp; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "mvt"
../../../bin/analyze -p mvt
time (for i in {1..5}; do ./mvt 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD mvt; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "nussinov"
../../../bin/analyze -p nussinov
time (for i in {1..5}; do ./nussinov 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD nussinov; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "seidel-2d"
../../../bin/analyze -p seidel-2d
time (for i in {1..5}; do ./seidel-2d 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD seidel-2d; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "symm"
../../../bin/analyze -p symm
time (for i in {1..5}; do ./symm 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD symm; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "syr2k"
../../../bin/analyze -p syr2k
time (for i in {1..5}; do ./syr2k 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD syr2k; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "syrk"
../../../bin/analyze -p syrk
time (for i in {1..5}; do ./syrk 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD syrk; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "trisolv"
../../../bin/analyze -p trisolv
time (for i in {1..5}; do ./trisolv 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD trisolv; done)
diff output.ref para.ref
rm output.ref
rm para.ref

echo "trmm"
../../../bin/analyze -p trmm
time (for i in {1..5}; do ./trmm 2> output.ref; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD trmm; done)
diff output.ref para.ref
rm output.ref
rm para.ref

