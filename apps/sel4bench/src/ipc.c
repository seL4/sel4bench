/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */
#include <autoconf.h>
#include <ipc.h>
#include <sel4bench/sel4bench.h>
#include <utils/util.h>

#include "benchmark.h"
#include "math.h"
#include "printing.h"
#include "processing.h"

static bool
process_ipc_results(ipc_results_t *raw_results, result_t *processed_results, int n)
{   
    /* calculate the overheads */
    ccnt_t overhead[NUM_OVERHEAD_BENCHMARKS];
    for (int i = 0; i < NUM_OVERHEAD_BENCHMARKS; i++) {
        if (!results_stable(raw_results->overhead_benchmarks[i], RUNS)) {
            printf("Benchmarking overhead of a %s is not stable! Cannot continue\n",
                    overhead_benchmark_params[i].name);
            print_all(raw_results->overhead_benchmarks[i], RUNS);
            if (config_set(CONFIG_ALLOW_UNSTABLE_OVERHEAD)) {
                return false;
            }
        }
        overhead[i] = results_min(raw_results->overhead_benchmarks[i], RUNS);
    }
      /* Take the smallest overhead to be our benchmarking overhead */
    raw_results->overheads[CALL_REPLY_RECV_OVERHEAD] = MIN(overhead[CALL_OVERHEAD],
                                                        overhead[REPLY_RECV_OVERHEAD]);
    raw_results->overheads[SEND_RECV_OVERHEAD] = MIN(overhead[SEND_OVERHEAD],
                                                   overhead[RECV_OVERHEAD]);
    raw_results->overheads[CALL_REPLY_RECV_10_OVERHEAD] = MIN(overhead[CALL_10_OVERHEAD],
                                                            overhead[REPLY_RECV_10_OVERHEAD]);

    /* now calculate the results (taking overheads into account */
    for (int i = 0; i < n; i++) {
        processed_results[i] = process_result(raw_results->benchmarks[i], RUNS,
                                              benchmark_params[i].name);
    }
    return true;
}

static void
print_single_ipc_xml_result(int result, ccnt_t value, char *name)
{

    printf("\t<result name=\"");
    printf("%sAS", benchmark_params[result].same_vspace ? "Intra" : "Inter");
    printf("-%s", benchmark_params[result].name);
    printf("(%d %s %d, size %d)\" ", benchmark_params[result].client_prio,
           benchmark_params[result].direction == DIR_TO ? "--&gt;" : "&lt;--",
           benchmark_params[result].server_prio, benchmark_params[result].length);
    printf("type=\"%s\">"CCNT_FORMAT"</result>\n", name, value);

}

static void
print_ipc_xml_results(result_t *results, int n)
{   
    printf("<results>\n");
    for (int i = 0; i < n; i++) {
        print_single_ipc_xml_result(i, results[i].min, "min");
        print_single_ipc_xml_result(i, results[i].max, "max");
        print_single_ipc_xml_result(i, (ccnt_t) results[i].mean, "mean");
        print_single_ipc_xml_result(i, (ccnt_t) results[i].stddev, "stdev");
    }
    printf("</results>\n");
}

/* for pasting into a spreadsheet or parsing */
static void
print_ipc_tsv_results(result_t *results, int n)
{   
    printf("Function\tDirection\tClient Prio\tServer Prio\tSame vspace?\tDummy (prio)?\tLength\t");
    print_result_header(); 
    for (int i = 0; i < n; i++) {
        printf("%s\t", benchmark_params[i].name);
        printf("%s\t", benchmark_params[i].direction == DIR_TO ? "client -> server" : "server -> client");
        printf("%d\t", benchmark_params[i].client_prio);
        printf("%d\t", benchmark_params[i].server_prio);
        printf("%s\t", benchmark_params[i].same_vspace ? "true" : "false");
        printf("%s (%d)\t", benchmark_params[i].dummy_thread ? "true" : "false", benchmark_params[i].dummy_prio);
        print_result(&results[i]);
    }
}

static void
print_ipc_results(result_t *results, format_t format, int n)
{
    switch (format) {
    case XML:
        print_ipc_xml_results(results, n);
        break;
    case TSV:
        print_ipc_tsv_results(results, n);
        break;
    default:
        ZF_LOGE("Unknown format");
        break;
    }
}

static void
process(void *results)
{
    /* Calculated results to print out */
    ipc_results_t *raw_results = (ipc_results_t *) results;
    int num_results = ARRAY_SIZE(benchmark_params);
    result_t processed_results[num_results];

    if (process_ipc_results(raw_results, processed_results, num_results)) {
        print_ipc_results(processed_results, XML, num_results);
        print_ipc_results(processed_results, TSV, num_results); 
    }
}

static benchmark_t ipc_benchmark = {
    .name = "ipc",
    .enabled = config_set(CONFIG_APP_IPCBENCH),
    .results_pages = BYTES_TO_SIZE_BITS_PAGES(sizeof(ipc_results_t), seL4_PageBits),
    .process = process,
    .init = blank_init
};

benchmark_t *
ipc_benchmark_new(void)
{
   return &ipc_benchmark;
}

