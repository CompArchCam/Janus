# Janus: Static Guided Dynamic Binary Modification #

Janus is a same-ISA dynamic binary modification tool that is controlled through static analysis.
It is developed at the University of Cambridge Computer Laboratory and available under an Apache licence.  If you use Janus in your work, please cite our [CGO 2019 publication](#publication).

Janus first performs an analysis of a binary executable to determine the transformations required. These transformations are then encoded into a series of rewrite rules specific to that binary.
Janus dynamic modifier is implemented as a client of [DynamoRIO](http://dynamorio.org/). It reads these rewrite rules and carries out the transformation as instructed when it encounters the relevant machine code.

![Overview](http://www.cl.cam.ac.uk/~rkz20/img/overview.png)

## What can Janus do ##
Janus is designed to perform sophisticated modifications and optimizations on generic x86-64 and AArch64 ELF binaries. It augments DynamoRIO with a static binary analyser and a runtime client. With a combination of static binary analysis and dynamic binary modification controlled by domain-specific rewrite rules (DSL), Janus is able to perform a series of tasks that other tools might not provide:

* End-to-End solutions to enable automatic parallelization, vectorization and software prefetch on executables
* Statically-guided binary instrumentation
* A platform for testing thread-level speculation on real systems

## What can future Janus do ##
We are still working on Janus to make it better and useful. Here are our objectives:

* A domain specific language and compiler to generate Janus rewrite rule definitions, static rule generation and dynamic rule interpreters automatically.
* A better binary alias analysis and interprocedural analysis
* Support different parallelism and schedule policies for automatic parallelization using the new DSL
* New tools such as: statically-guided softbound checker, parallel execution models, cache execution models for performance analysis


## Disclaimer ##
Janus is still in its prototype stage and it is not in the best production quality.
We cannot guarantee fault-free execution or parallelisation performance on binaries other than those provided,
although we expect that other binaries (especially small and simple binary) will work correctly in parallel.
And we do welcome anyone to contribute in this project and make this tool more useful. Please contact [Ruoyu Zhou](https://www.cl.cam.ac.uk/~rkz20/) if you have questions.

## Components ##
### Analyze ###
A static binary analyzer that examines input binaries and identifies opportunities for optimization. It then encodes the transformation in a domain specific rewrite schedule.

### JPar ###
JPar firstly performs a static analysis on the input binary using *analyze* and generates a rewrite schedule and performs automatic parallelization on the same binary.

### JVec ###
JVec firstly performs a static analysis on the input binary using *analyze* and generates a rewrite schedule and performs automatic vectorization on the same binary. Currently it is not fully operational.

### JFet ###
JFet firstly performs a static analysis on the input binary using *analyze* and generates a rewrite schedule and performs automatic memory prefetch on the same binary. Currently it is not fully operational.

### JITSTM ###
JITSTM is a software transactional memory that is generated at runtime. It redirects generic memory accesses to the speculative read/write buffers. The design is similar to [JudoSTM](https://ieeexplore.ieee.org/document/4336226). Currently it is not fully operational and its performance is not optimized.

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

## Run on your own executable ##
Once you add Janus into PATH, you can try Janus on your own binary:
```bash
jpar_all <num_threads> <executable> <arguments>
```
It runs Janus profiler, static analyzer, dynamic parallelizer in one go. It might take a while to do the profiling and analysis. At end it should generate a rewrite schedule so you can reuse it and invoke dynamic parallelizer directly. It is natural that you might find bugs, infinite loops and segfaults. We are slowly working on this to make Janus more robust and useful.

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

The list of rewrite rules can be found in "shared/rule_isa.h".

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
## Publication ##
[Janus: Statically-Driven and Profile-Guided Automatic Dynamic Binary Parallelisation](https://www.cl.cam.ac.uk/~rkz20/paper/cgo19janus.pdf)
Ruoyu Zhou and Timothy M. Jones 
International Symposium on Code Generation and Optimization (CGO), February 2019

The Janus Triad: Exploiting Parallelism Through Dynamic Binary Modification 
Ruoyu Zhou, George Wort, Márton Erdős and Timothy M. Jones 
International Conference on Virtual Execution Environments (VEE), April 2019

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
  * **core**: data structure definitions of core components, instruction, basic block, function etc.
  * **loader**: parse the input executable and retrieve the .text section
  * **arch**: architecture specific code
  * **analysis**: control, data flow and dependence analysis.
  * **schedgen**: generate rewrite schedules.

* The **dynamic** folder contains the source for the dynamic components, a client in DynamoRIO.
  * **core**: dynamic components to process rewrite schedules at runtime.
  * **parallel**: code for implementing the dynamic binary parallelizer.
  * **prefetcher**: code for implementing the dynamic binary prefetcher.

* The **shared** folder contains the interface shared between the static and dynamic components.
  * **rule_isa.h**: define your new rewrite rules here


### Debug ###
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

* `-DJANUS_DEBUG_RUN_ITERATION_END` : insert a SIGTRAP at each loop start and loop finish at main thread.
* `-DJANUS_DEBUG_RUN_THREADED` : insert a SIGTRAP when a new Janus thread is going to execute the loop.
* `-DJANUS_DEBUG_RUN_ITERATION_START` : insert a SIGTRAP at each start of the loop iteration. It applies to all the threads.
* `-DHALT_MAIN_THREAD` : make the main thread spins in a infinite loop when the loop starts. The rest of threads are not affected.
* `-DHALT_PARALLEL_THREAD` : make the parallel threads spins in a infinite loop when the loop starts. The main thread are not affected.
* `-JANUS_STATS` : collects the runtime stats for this parallel run. For example, the number of iterations, invocations, rollbacks, segfaults etc.


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

