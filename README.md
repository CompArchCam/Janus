# Janus: Static Guided Dynamic Binary Modification #

Janus is a same-ISA dynamic binary modification tool that is controlled through static analysis.
It first performs an analysis of a binary executable to determine the transformations required. These transformations are then encoded into a series of rewrite rules specific to that binary.
The Janus dynamic modifier, based on [DynamoRIO](http://dynamorio.org/), reads these rewrite rules and carries out the transformation as instructed when it encounters the relevant machine code.

![Overview](http://www.cl.cam.ac.uk/~rkz20/img/overview.png)

## Components ##
### Analyze ###
A static binary analyzer that examines input binaries and identifies opportunities for optimization. It then encodes the transformation in a domain specific rewrite schedule.

### JPar ###
JPar firstly performs a static analysis on the input binary using *analyze* and generates a rewrite schedule and performs automatic parallelization on the same binary.

### JVec ###
JVec firstly performs a static analysis on the input binary using *analyze* and generates a rewrite schedule and performs automatic vectorization on the same binary. Currently not yet released.

### JFet ###
JFet firstly performs a static analysis on the input binary using *analyze* and generates a rewrite schedule and performs automatic memory prefetch on the same binary. Currently not yet released.

## Installation ##
Janus uses [cmake](https://cmake.org/) for building its whole components. There is no need to install additional libraries. Simply create a new build folder and invoke cmake.

```bash
mkdir build
cd build
cmake ..
make -j
```
You can also build Janus with `VERBOSE` mode enabled.
```bash
cmake -DVERBOSE=ON ..
```

After building, there are several components generated:

* **bin/analyze**: the static binary analyzer
* **bin/schedump**: a utility tool to display the content of generated rewrite schedules
* **lib/libjpar.so**: a DynamoRIO client library for automatic parallelization (JPar)
* **lib/libjvec.so**: a DynamoRIO client library for automatic vectorization (JVec)
* **lib/libjpft.so**: a DynamoRIO client library for the automatic binary prefetcher (JPft)

There are a few convenient bash scripts in the **janus** folder.

* **janus/jpar**: run static analyzer and call the automatic parallelizer.
* **janus/jpar_debug**: run static analyzer and call the automatic parallelizer in debugging mode (gdb).
* **janus/jpar_all**: run profiler, static analyzer, dynamic parallelizer in one go (note that this will take lots of time).
* **janus/graph**: generate CFG graph of the binary in terms of loops or procedure as pdf.
* **janus/lcov**: generate the profiled coverage information of each static loop.
* **janus/plan**: run the dynamic dependence profiler.
* **janus/plan_train**: run the loop coverage profiler and dynamic dependence profiler together.
* **janus/run_many.sh**: measure the timing of the input command for 10 times.

For convenience, you can add these scripts into PATH:
```bash
export PATH=$PATH:${YOUR_PATH_TO_JANUS}/janus/
```
## Run correctness tests ##
Once it is built. You can test Janus on your x86 or AArch64 machine. Please ensure that you have at least four cores in your test machine.
Simply type:
```bash
make test
```
It runs the native binaries first and then runs Janus paralleliser with the same binary.


## Run a single test ##
To test a specific executable, you can simply invoke the corresponding Janus script.
```bash
jpar <num_threads> <executable> <arguments>
```
For example, to parallelise an executable "2mm" with 4 threads, you can simply type:
```bash
jpar 4 2mm
```

## Janus break down step by step ##
Janus performs static binary analysis and generates a rewrite schedule to guide the binary translation.
The static analyzer has lots of rule generation mode. 
```
Usage: analyze + <option> + <executable> + [profile_info]
Option:
  -a: static analysis without generating rules
  -cfg: generate CFG from the binary
  -p: generate rules for automatic parallelisation
  -lc: generate rules for loop coverage profiling
  -fc: generate rules for function coverage profiling (not yet working)
  -pr: generate rules for automatic loop profiling
  -o: generate rules for single thread optimization (not yet working)
  -v: generate rules for automatic vectorization (not yet working)
```

For example. you can run the static binary analyzer:
```bash
analyze -p 2mm
```
A **Janus Rewrite Schedule** (JRS) file "2mm.jrs" is generated.
This file is obfuscated but you can examine the contents using the "schedump" tool.
```bash
schedump 2mm.jrs
```
The rewrite schedule file is actually a list of "rewrite rules" to be interpreted by the dynamic binary translation tool.

The list of rewrite rules can be found in "shared/sched_rule.h".

It also generates the detailed report of the binary analysis in "2mm.loop.log" and "2mm.loop.alias.log".

If the rewrite rule exist along with the binary. You can invoke janus without re-generating new rewrite schedules
```bash
jpar 4 2mm
```
## Janus Static Analyzer Options ##
By using Janus static analyzer, you can generate different rewrite schedules from the same binary:
```bash
${YOUR_PATH_TO_JANUS}/bin/analyze <options> <binary>
```

## Disclaimer ##
Janus is still in its prototype stage and it is not in the best production quality.
We cannot guarantee fault-free execution or parallelisation performance on binaries other than those provided,
although we expect that other binaries (especially small and simple binary) will work correctly in parallel.
And we do welcome anyone to contribute in this project and make this tool more useful.

## Development Notes ##
### Documentation ###
The Janus development documentation can be built using *DoxyGen*. You can follow the steps described [here](https://www.stack.nl/~dimitri/doxygen/manual/install.html) to install *DoxyGen*.

Then run (in the root folder of the project):
```bash
doxygen Doxyfile
```

This generates the html documentation into the *docs/html* folder. Open *docs/html/index.html* to start browsing.

### Source structure ###

* The **static** folder contains the source for the Janalyze static binary analyser.
  * **core**: data structure definations of core components, instruction, basic block, function etc.
  * **loader**: parse the input executable and retrieve the .text section
  * **arch**: architecture specific code
  * **analysis**: control, data flow and dependence analysis.
  * **schedgen**: generate rewrite schedules.

* The **dynamic** folder contains the source for the dynamic components, a client in DynamoRIO.
  * **core**: dynamic components to process rewrite schedules at runtime.
  * **parallel**: code for implementing the dynamic binary parallelizer.
  * **prefetcher**: code for implementing the dynamic binary prefetcher.

* The **shared** folder contains the interface shared between the static and dyanmic components.
  * **sched_rule.h**: define your new rewrite rules here


### Debuggng ###
The debugging build will print many event information to help you debug.
```bash
cd build
cmake -DVERBOSE=ON ..
make -j
```
You can also invoke the jpar with gdb
```bash
${YOUR_PATH_TO_JANUS}/janus/jpar_debug <num_threads> <binary> <arguments>
```

You can also rebuild with the following macros to generate a SIGTRAP at specific locations:

* -DJANUS_DEBUG_RUN_ITERATION_END : insert a SIGTRAP at each loop start and loop finish at main thread.
* -DJANUS_DEBUG_RUN_THREADED : insert a SIGTRAP when a new Janus thread is going to execute the loop.
* -DJANUS_DEBUG_RUN_ITERATION_START : insert a SIGTRAP at each start of the loop iteration. It applies to all the threads.
* -DHALT_MAIN_THREAD : make the main thread spins in a infinite loop when the loop starts. The rest of threads are not affected.
* -DHALT_PARALLEL_THREAD : make the parallel threads spins in a infinite loop when the loop starts. The main thread are not affected.
* -JANUS_STATS : collects the runtime stats for this parallel run. For example, the number of iterations, invocations, rollbacks, segfaults etc.


### How to visualise loops in your executable ###
Janus static analyser has -a mode which can dump the information it retrieves from the static analysis.
It generates the CFG as the dot format so that you can visualise the CFG in the pdf format.
From the pdf you can understand the binary better.
```bash
sudo apt-get install graphviz pdftk

#run static analyser
analyze -a exe
#run loop cfg generation
graph exe.loop.cfg
#run loop ssa generation
graph exe.loop.ssa
#run function cfg generation
graph exe.proc.cfg
#run function ssa generation
graph exe.proc.ssa
#print the output pdf
evince exe.loop.cfg.pdf &
```

