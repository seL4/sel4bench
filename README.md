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

This is a hot cache benchmark of the irq path, measured from inside the kernel. It requires [tracepoints](https://wiki.sel4.systems/Benchmarking%20guide#In_kernel_log-buffer) to be placed on the irq path where the meaurements are to be taken from.

## irquser

This is a hot cache benchmark of the irq path, measured from user-level.

## scheduler

This is a hot cache benchmark of a scheduling decision. It works by using a producer-consumer pattern between two notification objects. 
This benchmark also measures `seL4_Yield`

## signal

This is a hot cache benchmark of the signal path in the kernel, measured from user level. 
