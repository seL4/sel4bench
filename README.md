<!--
  Copyright 2017, Data61
  Commonwealth Scientific and Industrial Research Organisation (CSIRO)
  ABN 41 687 119 230.

  This software may be distributed and modified according to the terms of
  the BSD 2-Clause license. Note that NO WARRANTY is provided.
  See "LICENSE_BSD2.txt" for details.

  @TAG(DATA61_BSD)
-->
# sel4bench

sel4 benchmarking applications and support library.

To get this project, check out the project manifest: https://github.com/seL4/sel4bench-manifest

# Applications

We provide multiple applications for benchmarking different paths in the kernel.

## sel4bench

This is the driver application: it launches each benchmark in a separate process and collects, processes and outputs results.

## ipc

This is a hot cache benchmark of the IPC path.

## irq

This is a hot cache benchmark of the irq path, measured from inside the kernel. It requires [tracepoints](https://docs.sel4.systems/BenchmarkingGuide.html#in-kernel-log-buffer) to be placed on the irq path where the meaurements are to be taken from.

## irquser

This is a hot cache benchmark of the irq path, measured from user-level.

## scheduler

This is a hot cache benchmark of a scheduling decision. It works by using a producer-consumer pattern between two notification objects.
This benchmark also measures `seL4_Yield`

## signal

This is a hot cache benchmark of the signal path in the kernel, measured from user level.

## smp

This is an intra-core ipc round-trip benchmark to check overhead of the kernel synchronization on ipc throughput.

## vcpu

In order to run this benchmark, you must notify the build system that you wish to enable this benchmark by passing `-DVCPU=true` on the command line, which will cause the kernel to be compiled to run in EL2. You must also ensure that you pass `-DHARDWARE=false` to disable the hardware tests.

Since this benchmark will cause the kernel image to be an EL2 image, it will have an impact on the observed numbers for the other benchmark applications as well since they'll be using an unexpected kernel build.

This benchmark will execute a thread as a VCPU (an EL1 guest kernel) and then obtain numbers for the following actions:
* Privilege escalation from EL1 to EL2 using the HVC instruction.
* Privilege de-escalation from EL2 to EL1 using the ERET instruction.
* The cost of a NULL invocation of the EL2 kernel using HVC.
* The cost of an seL4_Call from an EL1 guest thread to a native seL4 thread.
* The cost of an seL4_Reply from an seL4 native thread to an EL1 guest thread.
