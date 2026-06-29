<!--
     Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)

     SPDX-License-Identifier: BSD-2-Clause
-->

# The sel4bench repository

_sel4bench_ is a benchmarking applications and support library for [seL4].

To work with this project, check out the [project manifest].

For past and current data collected by these benchmarks on a variety of
boards, see the repository [seL4/sel4bench-results]

For current results see also the [performance page] on the seL4 website.

## Applications

This repository is structured as multiple separate applications which contain
benchmarks for different paths in the kernel.

After finishing, the benchmark produces JSON output on stdout delimited by
the strings `BEGIN JSON OUPUT`, `END JSON OUPUT`.

All benchmarks report mean, stddev, and n. Some benchmarks additionally produce
a raw result array, and the statistics data min, max, median, Q1, and Q3.

### sel4bench

This is the driver application: it launches each benchmark in a separate
process and collects, processes, and outputs results.

### ipc

This is a hot-cache benchmark of various IPC paths.

### irquser

This is a hot-cache benchmark of various IRQ paths, measured from user space.

### scheduler

This is a hot-cache benchmark for scheduling decisions.  It works by
using a producer/consumer pattern between two notification objects.
This benchmark also measures `seL4_Yield()`.

### signal

This is a hot-cache benchmark of the signal paths in the kernel, measured
from user space.

### smp

This is an intra-core IPC round-trip benchmark to check overhead of kernel
synchronisation on IPC throughput. It measures overall IPC throughput under
different simulated workloads where each core makes same-core IPC calls. The
expectation is that for light work loads the throughput scales linearly in the
number of cores, and that for high workloads, there is a limit in the number of
cores where the throughput no longer scales linearly because of kernel lock
contention.

### vcpu (AArch64 only)

This benchmark executes a thread as a VCPU (an EL1 guest kernel) and then obtains
numbers for the following actions:

* Privilege escalation from EL1 to EL2 using the `HVC` instruction.
* Privilege de-escalation from EL2 to EL1 using the `ERET` instruction.
* The cost of a null invocation of the EL2 kernel using `HVC`.
* The cost of an `seL4_Call()` from an EL1 guest thread to a native seL4
  thread.
* The cost of an `seL4_Reply()` from an seL4 native thread to an EL1
  guest thread.

**Note:** In order to run this benchmark, you must enable it at build time by
passing `-DVCPU=true` on the `init` or `cmake` command line, which will cause
the kernel to be compiled to run in EL2. You must also ensure that you pass
`-DHARDWARE=false` to disable the hardware tests.

Since this benchmark will cause the kernel image to be an EL2 image, it will
have an impact on the observed numbers for the other benchmark applications as
well. The overhead calculations and other assumptions other benchmarks are
making may not be valid for EL2 kernels.


## Adding a new benchmark

Contributing a new benchmark to seL4bench requires a few steps:

* Under `apps`, create a directory for your new benchmark and:
  * Provide a `CMakelists.txt` file that defines a new executable.
  * Provide a `src` folder that contains the source code for your benchmark.
* Under `apps/sel4bench`:
  * Update `CMakeLists.txt` to add your new benchmark to the list of benchmarks.
  * Under `src`:
    * Update `benchmark.h` to include your generated config for your benchmark,
      and provide a function declaration that will act as the entry point for
      your benchmark.
    * Provide a `<benchmark_name>.c` file that implements the above function
      declaration. This function should return a `benchmark_t` struct. Construct
      this struct accordingly. The struct expects a function to process the
      results of the benchmark, which you should provide in this file as well
    * Inside `main.c`, add your entry point function that was declared/defined
      above to the array of `benchmark_t` present.
* Update `easy-settings.cmake` to add your new benchmark. You can define here
  whether the benchmark should be enabled by default or not.
* Under `libsel4benchsupport/include`:
  * Provide a `<benchmark_name.h>` file that provides any extra definitions that
    your benchmark may need. You will also generally provide a
    `benchmark_name_results_t` struct here, which will be used to store the
    results of your benchmark when processing.
* Update `settings.cmake` to include your new benchmark.

[seL4]: https://sel4.systems/
[seL4/sel4bench-results]: https://github.com/seL4/sel4bench-results
[project manifest]: https://github.com/seL4/sel4bench-manifest
[performance page]: https://sel4.systems/performance.html
