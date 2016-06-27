/*
* Copyright 2016, NICTA
*
* This software may be distributed and modified according to the terms of
* the GNU General Public License version 2. Note that NO WARRANTY is provided.
* See "LICENSE_GPLv2.txt" for details.
*
* @TAG(NICTA_GPL)
*/
#include "json.h"

static void
result_to_json(result_t result, json_t *j)
{
   UNUSED int error = json_object_set_new(j, "Min", json_integer(result.min));
   assert(error == 0);

   error = json_object_set_new(j, "Max", json_integer(result.max));
   assert(error == 0);

   error = json_object_set_new(j, "Mean", json_real(result.mean));
   assert(error == 0);

   error = json_object_set_new(j, "Stddev", json_real(result.stddev));
   assert(error == 0);

   error = json_object_set_new(j, "Variance", json_real(result.variance));
   assert(error == 0);

   error = json_object_set_new(j, "Mode", json_real(result.mode));
   assert(error == 0);

   error = json_object_set_new(j, "Median", json_real(result.median));
   assert(error == 0);

   error = json_object_set_new(j, "1st quantile", json_real(result.first_quantile));
   assert(error == 0);

   error = json_object_set_new(j, "3rd quantile", json_real(result.third_quantile));
   assert(error == 0);

   error = json_object_set_new(j, "Samples", json_integer(result.samples));
   assert(error == 0);

   json_t *raw_results = json_array();
   assert(raw_results != NULL);

   for (size_t i = 0; i < result.samples; i++) {
       error = json_array_append_new(raw_results, json_integer(result.raw_data[i]));
       assert(error == 0);
   }
   json_object_set_new(j, "Raw results", raw_results);
   assert(error == 0);
}

static json_t *
get_json_cell(column_t column, size_t index)
{
    switch (column.type) {
    case JSON_STRING:
        return json_string(column.string_array[index]);
    case JSON_INTEGER:
        return json_integer(column.integer_array[index]);
    case JSON_REAL:
        return json_real(column.real_array[index]);
    case JSON_TRUE:
    case JSON_FALSE:
        return json_boolean(column.bool_array[index]);
    default:
        ZF_LOGE("Columns of type %d not supported", column.type);
        return json_null();
    }
}

json_t *
result_set_to_json(result_set_t set)
{
   UNUSED int error;
   json_t *object = json_object();
   assert(object != NULL);

   error = json_object_set_new(object, "Benchmark", json_string(set.name));
   assert(error == 0);

   json_t *rows = json_array();
   assert(rows != NULL);

   error = json_object_set_new(object, "Results", rows);
   assert(error == 0);

   for (int i = 0; i < set.n_results; i++) {
       json_t *row = json_object();
       for (int c = 0; c < set.n_extra_cols; c++) {
           assert(set.extra_cols != NULL);
           json_t *cell = get_json_cell(set.extra_cols[c], i);
           assert(cell != NULL);

           error = json_object_set_new(row, set.extra_cols[c].header, cell);
           assert(error == 0);

        }
        result_to_json(set.results[i], row);

        error = json_array_append_new(rows, row);
        assert(error == 0);
    }

   return object;
}
