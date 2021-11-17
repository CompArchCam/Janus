#!/bin/bash

my_dir="$(dirname "$0")"
source $my_dir/janus_header

function usage {
    echo "Run performance test shown from the paper"
    echo "Usage: "
    echo "./run_polybench_test <number_of_threads>"
}

if [ $# -lt 1 ]
then 
  usage
  exit
fi

num_thread=$1

#switch to older version of dynamoRIO for paralleliser
TESTDIR="$DIR/../tests"
JANUSRUN="$DIR/run-many.sh"

cd $TESTDIR/polybench/gcc-native

echo "2mm.native"
$JANUSRUN ./2mm
echo "2mm.static"
$DIR/../bin/analyze -p 2mm
echo "2mm.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- 2mm
echo "2mm.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./2mm

echo "3mm.native"
$JANUSRUN ./3mm
echo "3mm.static"
$DIR/../bin/analyze -p 3mm
echo "3mm.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- 3mm
echo "3mm.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./3mm

echo "adi.native"
$JANUSRUN ./adi
echo "adi.static"
$DIR/../bin/analyze -p adi
echo "adi.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- adi
echo "adi.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./adi

echo "atax.native"
$JANUSRUN ./atax
echo "atax.static"
$DIR/../bin/analyze -p atax
echo "atax.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- atax
echo "atax.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./atax

echo "bicg.native"
$JANUSRUN ./bicg
echo "bicg.static"
$DIR/../bin/analyze -p bicg
echo "bicg.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- bicg
echo "bicg.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./bicg

echo "cholesky.native"
$JANUSRUN ./cholesky
echo "cholesky.static"
$DIR/../bin/analyze -p cholesky
echo "cholesky.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- cholesky
echo "cholesky.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./cholesky

echo "correlation.native"
$JANUSRUN ./correlation
echo "correlation.static"
$DIR/../bin/analyze -p correlation
echo "correlation.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- correlation
echo "correlation.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./correlation

echo "covariance.native"
$JANUSRUN ./covariance
echo "covariance.static"
$DIR/../bin/analyze -p covariance
echo "covariance.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- covariance
echo "covariance.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./covariance

echo "deriche.native"
$JANUSRUN ./deriche
echo "deriche.static"
$DIR/../bin/analyze -p deriche
echo "deriche.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- deriche
echo "deriche.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./deriche

echo "doitgen.native"
$JANUSRUN ./doitgen
echo "doitgen.static"
$DIR/../bin/analyze -p doitgen
echo "doitgen.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- doitgen
echo "doitgen.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./doitgen

echo "durbin.native"
$JANUSRUN ./durbin
echo "durbin.static"
$DIR/../bin/analyze -p durbin
echo "durbin.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- durbin
echo "durbin.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./durbin

echo "fdtd-2d.native"
$JANUSRUN ./fdtd-2d
echo "fdtd-2d.static"
$DIR/../bin/analyze -p fdtd-2d
echo "fdtd-2d.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- fdtd-2d
echo "fdtd-2d.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./fdtd-2d

echo "floyd-warshall.native"
$JANUSRUN ./floyd-warshall
echo "floyd-warshall.static"
$DIR/../bin/analyze -p floyd-warshall
echo "floyd-warshall.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- floyd-warshall
echo "floyd-warshall.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./floyd-warshall

echo "gemm.native"
$JANUSRUN ./gemm
echo "gemm.static"
$DIR/../bin/analyze -p gemm
echo "gemm.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- gemm
echo "gemm.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./gemm

echo "gemver.native"
$JANUSRUN ./gemver
echo "gemver.static"
$DIR/../bin/analyze -p gemver
echo "gemver.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- gemver
echo "gemver.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./gemver

echo "gesummv.native"
$JANUSRUN ./gesummv
echo "gesummv.static"
$DIR/../bin/analyze -p gesummv
echo "gesummv.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- gesummv
echo "gesummv.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./gesummv

echo "gramschmidt.native"
$JANUSRUN ./gramschmidt
echo "gramschmidt.static"
$DIR/../bin/analyze -p gramschmidt
echo "gramschmidt.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- gramschmidt
echo "gramschmidt.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./gramschmidt

echo "heat-3d.native"
$JANUSRUN ./heat-3d
echo "heat-3d.static"
$DIR/../bin/analyze -p heat-3d
echo "heat-3d.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- heat-3d
echo "heat-3d.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./heat-3d

echo "jacobi-1d.native"
$JANUSRUN ./jacobi-1d
echo "jacobi-1d.static"
$DIR/../bin/analyze -p jacobi-1d
echo "jacobi-1d.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- jacobi-1d
echo "jacobi-1d.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./jacobi-1d

echo "jacobi-2d.native"
$JANUSRUN ./jacobi-2d
echo "jacobi-2d.static"
$DIR/../bin/analyze -p jacobi-2d
echo "jacobi-2d.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- jacobi-2d
echo "jacobi-2d.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./jacobi-2d

echo "lu.native"
$JANUSRUN ./lu
echo "lu.static"
$DIR/../bin/analyze -p lu
echo "lu.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- lu
echo "lu.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./lu

echo "ludcmp.native"
$JANUSRUN ./ludcmp
echo "ludcmp.static"
$DIR/../bin/analyze -p ludcmp
echo "ludcmp.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- ludcmp
echo "ludcmp.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./ludcmp

echo "mvt.native"
$JANUSRUN ./mvt
echo "mvt.static"
$DIR/../bin/analyze -p mvt
echo "mvt.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- mvt
echo "mvt.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./mvt

echo "nussinov.native"
$JANUSRUN ./nussinov
echo "nussinov.static"
$DIR/../bin/analyze -p nussinov
echo "nussinov.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- nussinov
echo "nussinov.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./nussinov

echo "seidel-2d.native"
$JANUSRUN ./seidel-2d
echo "seidel-2d.static"
$DIR/../bin/analyze -p seidel-2d
echo "seidel-2d.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- seidel-2d
echo "seidel-2d.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./seidel-2d

echo "symm.native"
$JANUSRUN ./symm
echo "symm.static"
$DIR/../bin/analyze -p symm
echo "symm.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- symm
echo "symm.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./symm

echo "syr2k.native"
$JANUSRUN ./syr2k
echo "syr2k.static"
$DIR/../bin/analyze -p syr2k
echo "syr2k.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- syr2k
echo "syr2k.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./syr2k

echo "syrk.native"
$JANUSRUN ./syrk
echo "syrk.static"
$DIR/../bin/analyze -p syrk
echo "syrk.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- syrk
echo "syrk.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./syrk

echo "trisolv.native"
$JANUSRUN ./trisolv
echo "trisolv.static"
$DIR/../bin/analyze -p trisolv
echo "trisolv.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- trisolv
echo "trisolv.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./trisolv

echo "trmm.native"
$JANUSRUN ./trmm
echo "trmm.static"
$DIR/../bin/analyze -p trmm
echo "trmm.dbt"
$JANUSRUN $DIR/../external/DynamoRIO-Linux-5.1.0-RC1/bin64/drrun -- trmm
echo "trmm.parallel"
$JANUSRUN $DIR/jpar_dyn $num_thread ./trmm
