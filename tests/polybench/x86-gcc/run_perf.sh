#!/bin/bash
THREAD=4

echo "2mm"
../../../bin/analyze -p 2mm
time (for i in {1..5}; do ./2mm; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- 2mm; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD 2mm; done)

echo "3mm"
../../../bin/analyze -p 3mm
time (for i in {1..5}; do ./3mm; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- 3mm; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD 3mm; done)

echo "adi"
../../../bin/analyze -p adi
time (for i in {1..5}; do ./adi; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- adi; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD adi; done)

echo "atax"
../../../bin/analyze -p atax
time (for i in {1..5}; do ./atax; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- atax; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD atax; done)

echo "bicg"
../../../bin/analyze -p bicg
time (for i in {1..5}; do ./bicg; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- bicg; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD bicg; done)

echo "cholesky"
../../../bin/analyze -p cholesky
time (for i in {1..5}; do ./cholesky; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- cholesky; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD cholesky; done)

echo "correlation"
../../../bin/analyze -p correlation
time (for i in {1..5}; do ./correlation; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- correlation; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD correlation; done)

echo "covariance"
../../../bin/analyze -p covariance
time (for i in {1..5}; do ./covariance; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- covariance; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD covariance; done)

echo "deriche"
../../../bin/analyze -p deriche
time (for i in {1..5}; do ./deriche; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- deriche; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD deriche; done)

echo "doitgen"
../../../bin/analyze -p doitgen
time (for i in {1..5}; do ./doitgen; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- doitgen; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD doitgen; done)

echo "durbin"
../../../bin/analyze -p durbin
time (for i in {1..5}; do ./durbin; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- durbin; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD durbin; done)

echo "fdtd-2d"
../../../bin/analyze -p fdtd-2d
time (for i in {1..5}; do ./fdtd-2d; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- fdtd-2d; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD fdtd-2d; done)

echo "floyd-warshall"
../../../bin/analyze -p floyd-warshall
time (for i in {1..5}; do ./floyd-warshall; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- floyd-warshall; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD floyd-warshall; done)

echo "gemm"
../../../bin/analyze -p gemm
time (for i in {1..5}; do ./gemm; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- gemm; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD gemm; done)

echo "gemver"
../../../bin/analyze -p gemver
time (for i in {1..5}; do ./gemver; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- gemver; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD gemver; done)

echo "gesummv"
../../../bin/analyze -p gesummv
time (for i in {1..5}; do ./gesummv; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- gesummv; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD gesummv; done)

echo "gramschmidt"
../../../bin/analyze -p gramschmidt
time (for i in {1..5}; do ./gramschmidt; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- gramschmidt; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD gramschmidt; done)

echo "heat-3d"
../../../bin/analyze -p heat-3d
time (for i in {1..5}; do ./heat-3d; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- heat-3d; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD heat-3d; done)

echo "jacobi-1d"
../../../bin/analyze -p jacobi-1d
time (for i in {1..5}; do ./jacobi-1d; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- jacobi-1d; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD jacobi-1d; done)

echo "jacobi-2d"
../../../bin/analyze -p jacobi-2d
time (for i in {1..5}; do ./jacobi-2d; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- jacobi-2d; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD jacobi-2d; done)

echo "lu"
../../../bin/analyze -p lu
time (for i in {1..5}; do ./lu; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- lu; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD lu; done)

echo "ludcmp"
../../../bin/analyze -p ludcmp
time (for i in {1..5}; do ./ludcmp; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- ludcmp; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD ludcmp; done)

echo "mvt"
../../../bin/analyze -p mvt
time (for i in {1..5}; do ./mvt; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- mvt; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD mvt; done)

echo "nussinov"
../../../bin/analyze -p nussinov
time (for i in {1..5}; do ./nussinov; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- nussinov; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD nussinov; done)

echo "seidel-2d"
../../../bin/analyze -p seidel-2d
time (for i in {1..5}; do ./seidel-2d; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- seidel-2d; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD seidel-2d; done)

echo "symm"
../../../bin/analyze -p symm
time (for i in {1..5}; do ./symm; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- symm; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD symm; done)

echo "syr2k"
../../../bin/analyze -p syr2k
time (for i in {1..5}; do ./syr2k; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- syr2k; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD syr2k; done)

echo "syrk"
../../../bin/analyze -p syrk
time (for i in {1..5}; do ./syrk; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- syrk; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD syrk; done)

echo "trisolv"
../../../bin/analyze -p trisolv
time (for i in {1..5}; do ./trisolv; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- trisolv; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD trisolv; done)

echo "trmm"
../../../bin/analyze -p trmm
time (for i in {1..5}; do ./trmm; done)
time (for i in {1..5}; do ../../../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- trmm; done)
time (for i in {1..5}; do ../../../gbr/gbp_dyn $THREAD trmm; done)

