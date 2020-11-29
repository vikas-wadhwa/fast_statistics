#include "fast_statistics.h"

static VALUE mFastStatistics;

#ifdef HAVE_SIMD_INTRINSICS
#define mm_get_index_float(packed, index) *(((float*)&packed) + index); 
#define mm_get_index_double(packed, index) *(((double*)&packed) + index); 
#endif

#ifdef HAVE_XMMINTRIN_H
inline __m128d pack_float64_m128(VALUE cols[], int row_index)
{
  __m128d packed = _mm_set_pd(
    NUM2DBL(rb_ary_entry(cols[0], row_index)),
    NUM2DBL(rb_ary_entry(cols[1], row_index))
  );
  return packed;
}

inline __m128 pack_float32_m128(VALUE cols[], int row_index)
{
  __m128 packed = _mm_set_ps(
    (float)NUM2DBL(rb_ary_entry(cols[0], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[1], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[2], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[3], row_index))
  );
  return packed;
}

VALUE descriptive_statistics_packed128_float64(VALUE self, VALUE arrays)
{
  Check_Type(arrays, T_ARRAY);

  int cols =  rb_array_len(arrays);
  int rows = rb_array_len(rb_ary_entry(arrays, 0));
  int simd_pack_size = 2;

  VALUE a_results  = rb_ary_new();
  VALUE s_min = ID2SYM(rb_intern("min"));
  VALUE s_max = ID2SYM(rb_intern("max"));
  VALUE s_mean = ID2SYM(rb_intern("mean"));
  VALUE s_variance = ID2SYM(rb_intern("variance"));
  VALUE s_standard_deviation = ID2SYM(rb_intern("standard_deviation"));

  for (int variable_index = 0; variable_index < cols; variable_index += simd_pack_size) {
    // Pack values in opposite order to maintain expected ruby order
    VALUE cols[2] = {
      rb_ary_entry(arrays, variable_index + 1),
      rb_ary_entry(arrays, variable_index + 0),
    };

    __m128d sums = _mm_setzero_pd();
    __m128d means = _mm_setzero_pd();
    __m128d mins = _mm_set_pd1(FLT_MAX);
    __m128d maxes = _mm_set_pd1(FLT_MIN);
    __m128d variances = _mm_setzero_pd();
    __m128d standard_deviations = _mm_setzero_pd();
    __m128d lengths = _mm_set_pd1((float) rows);

    for (int row_index = 0; row_index < rows; row_index++) {
      __m128d packed = pack_float64_m128(cols, row_index);
      sums = _mm_add_pd(sums, packed);
      mins = _mm_min_pd(mins, packed);
      maxes = _mm_max_pd(maxes, packed);
    }
    means = _mm_div_pd(sums, lengths);

    for (int row_index = 0; row_index < rows; row_index++) {
      __m128d packed = pack_float64_m128(cols, row_index);
      __m128d deviation = _mm_sub_pd(packed, means);
      __m128d sqr_deviation = _mm_mul_pd(deviation, deviation);
      variances = _mm_add_pd(variances, _mm_div_pd(sqr_deviation, lengths));
    }
    standard_deviations = _mm_sqrt_pd(variances);


    for (int simd_slot_index = 0; simd_slot_index < simd_pack_size; simd_slot_index++) {
      VALUE h_result = rb_hash_new();

      double min = mm_get_index_double(mins, simd_slot_index);
      double max = mm_get_index_double(maxes, simd_slot_index);
      double mean = mm_get_index_double(means, simd_slot_index);
      double variance = mm_get_index_double(variances, simd_slot_index);
      double standard_deviation = mm_get_index_double(standard_deviations, simd_slot_index);

      rb_hash_aset(h_result, s_min, DBL2NUM(min));
      rb_hash_aset(h_result, s_max, DBL2NUM(max));
      rb_hash_aset(h_result, s_mean, DBL2NUM(mean));
      rb_hash_aset(h_result, s_variance, DBL2NUM(variance));
      rb_hash_aset(h_result, s_standard_deviation, DBL2NUM(standard_deviation));

      rb_ary_push(a_results, h_result);
    }
  }

  return a_results;
}

VALUE descriptive_statistics_packed128_float32(VALUE self, VALUE arrays)
{
  Check_Type(arrays, T_ARRAY);

  int cols = rb_array_len(arrays);
  int rows = rb_array_len(rb_ary_entry(arrays, 0));
  int simd_pack_size = 4;

  VALUE a_results  = rb_ary_new();
  VALUE s_min = ID2SYM(rb_intern("min"));
  VALUE s_max = ID2SYM(rb_intern("max"));
  VALUE s_mean = ID2SYM(rb_intern("mean"));
  VALUE s_variance = ID2SYM(rb_intern("variance"));
  VALUE s_standard_deviation = ID2SYM(rb_intern("standard_deviation"));

  for (int variable_index = 0; variable_index < cols; variable_index += simd_pack_size) {
    // Pack values in opposite order to maintain expected ruby order
    VALUE cols[4] = {
      rb_ary_entry(arrays, variable_index + 3),
      rb_ary_entry(arrays, variable_index + 2),
      rb_ary_entry(arrays, variable_index + 1),
      rb_ary_entry(arrays, variable_index + 0),
    };

    __m128 sums = _mm_setzero_ps();
    __m128 means = _mm_setzero_ps();
    __m128 mins = _mm_set_ps1(FLT_MAX);
    __m128 maxes = _mm_set_ps1(FLT_MIN);
    __m128 variances = _mm_setzero_ps();
    __m128 standard_deviations = _mm_setzero_ps();
    __m128 lengths = _mm_set_ps1((float) rows);

    for (int row_index = 0; row_index < rows; row_index++) {
      __m128 packed = pack_float32_m128(cols, row_index);
      sums = _mm_add_ps(sums, packed);
      mins = _mm_min_ps(mins, packed);
      maxes = _mm_max_ps(maxes, packed);
    }
    means = _mm_div_ps(sums, lengths);

    for (int row_index = 0; row_index < rows; row_index++) {
      __m128 packed = pack_float32_m128(cols, row_index);
      __m128 deviation = _mm_sub_ps(packed, means);
      __m128 sqr_deviation = _mm_mul_ps(deviation, deviation);
      variances = _mm_add_ps(variances, _mm_div_ps(sqr_deviation, lengths));
    }
    standard_deviations = _mm_sqrt_ps(variances);


    for (int simd_slot_index = 0; simd_slot_index < simd_pack_size; simd_slot_index++) {
      VALUE h_result = rb_hash_new();

      double min = mm_get_index_float(mins, simd_slot_index);
      double max = mm_get_index_float(maxes, simd_slot_index);
      double mean = mm_get_index_float(means, simd_slot_index);
      double variance = mm_get_index_float(variances, simd_slot_index);
      double standard_deviation = mm_get_index_float(standard_deviations, simd_slot_index);

      rb_hash_aset(h_result, s_min, DBL2NUM(min));
      rb_hash_aset(h_result, s_max, DBL2NUM(max));
      rb_hash_aset(h_result, s_mean, DBL2NUM(mean));
      rb_hash_aset(h_result, s_variance, DBL2NUM(variance));
      rb_hash_aset(h_result, s_standard_deviation, DBL2NUM(standard_deviation));

      rb_ary_push(a_results, h_result);
    }
  }

  return a_results;
}

VALUE simd_enabled(VALUE self)
{
  return Qtrue;
}
#endif

#ifdef HAVE_IMMINTRIN_H
inline __m256d pack_float64_m256(VALUE cols[], int row_index)
{
  __m256d packed = _mm256_set_pd(
    NUM2DBL(rb_ary_entry(cols[0], row_index)),
    NUM2DBL(rb_ary_entry(cols[1], row_index)),
    NUM2DBL(rb_ary_entry(cols[2], row_index)),
    NUM2DBL(rb_ary_entry(cols[3], row_index))
  );
  return packed;
}

inline __m256 pack_float32_m256(VALUE cols[], int row_index)
{
  __m256 packed = _mm256_set_ps(
    (float)NUM2DBL(rb_ary_entry(cols[0], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[1], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[2], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[3], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[4], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[5], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[6], row_index)),
    (float)NUM2DBL(rb_ary_entry(cols[7], row_index))
  );
  return packed;
}

VALUE descriptive_statistics_packed256_float64(VALUE self, VALUE arrays)
{
  Check_Type(arrays, T_ARRAY);

  int cols = rb_array_len(arrays);
  int rows = rb_array_len(rb_ary_entry(arrays, 0));
  int simd_pack_size = 4;

  VALUE a_results  = rb_ary_new();
  VALUE s_min = ID2SYM(rb_intern("min"));
  VALUE s_max = ID2SYM(rb_intern("max"));
  VALUE s_mean = ID2SYM(rb_intern("mean"));
  VALUE s_variance = ID2SYM(rb_intern("variance"));
  VALUE s_standard_deviation = ID2SYM(rb_intern("standard_deviation"));

  for (int variable_index = 0; variable_index < cols; variable_index += simd_pack_size) {
    // Pack values in opposite order to maintain expected ruby order
    VALUE cols[4] = {
      rb_ary_entry(arrays, variable_index + 3),
      rb_ary_entry(arrays, variable_index + 2),
      rb_ary_entry(arrays, variable_index + 1),
      rb_ary_entry(arrays, variable_index + 0),
    };

    __m256d sums = _mm256_setzero_pd();
    __m256d means = _mm256_setzero_pd();
    __m256d mins = _mm256_set_pd(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
    __m256d maxes = _mm256_set_pd(FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN);
    __m256d variances = _mm256_setzero_pd();
    __m256d standard_deviations = _mm256_setzero_pd();
    double rows_d = (double)rows;
    __m256d lengths = _mm256_set_pd(rows_d, rows_d, rows_d, rows_d);

    for (int row_index = 0; row_index < rows; row_index++) {
      __m256d packed = pack_float64_m256(cols, row_index);
      sums = _mm256_add_pd(sums, packed);
      mins = _mm256_min_pd(mins, packed);
      maxes = _mm256_max_pd(maxes, packed);
    }
    means = _mm256_div_pd(sums, lengths);

    for (int row_index = 0; row_index < rows; row_index++) {
      __m256d packed = pack_float64_m256(cols, row_index);
      __m256d deviation = _mm256_sub_pd(packed, means);
      __m256d sqr_deviation = _mm256_mul_pd(deviation, deviation);
      variances = _mm256_add_pd(variances, _mm256_div_pd(sqr_deviation, lengths));
    }
    standard_deviations = _mm256_sqrt_pd(variances);

    for (int simd_slot_index = 0; simd_slot_index < simd_pack_size; simd_slot_index++) {
      VALUE h_result = rb_hash_new();

      double min = mm_get_index_double(mins, simd_slot_index);
      double max = mm_get_index_double(maxes, simd_slot_index);
      double mean = mm_get_index_double(means, simd_slot_index);
      double variance = mm_get_index_double(variances, simd_slot_index);
      double standard_deviation = mm_get_index_double(standard_deviations, simd_slot_index);

      rb_hash_aset(h_result, s_min, DBL2NUM(min));
      rb_hash_aset(h_result, s_max, DBL2NUM(max));
      rb_hash_aset(h_result, s_mean, DBL2NUM(mean));
      rb_hash_aset(h_result, s_variance, DBL2NUM(variance));
      rb_hash_aset(h_result, s_standard_deviation, DBL2NUM(standard_deviation));

      rb_ary_push(a_results, h_result);
    }
  }

  return a_results;
}

VALUE descriptive_statistics_packed256_float32(VALUE self, VALUE arrays)
{
  Check_Type(arrays, T_ARRAY);

  int cols = rb_array_len(arrays);
  int rows = rb_array_len(rb_ary_entry(arrays, 0));
  int simd_pack_size = 8;

  VALUE a_results  = rb_ary_new();
  VALUE s_min = ID2SYM(rb_intern("min"));
  VALUE s_max = ID2SYM(rb_intern("max"));
  VALUE s_mean = ID2SYM(rb_intern("mean"));
  VALUE s_variance = ID2SYM(rb_intern("variance"));
  VALUE s_standard_deviation = ID2SYM(rb_intern("standard_deviation"));

  for (int variable_index = 0; variable_index < cols; variable_index += simd_pack_size) {
    // Pack values in opposite order to maintain expected ruby order
    VALUE cols[8] = {
      rb_ary_entry(arrays, variable_index + 7),
      rb_ary_entry(arrays, variable_index + 6),
      rb_ary_entry(arrays, variable_index + 5),
      rb_ary_entry(arrays, variable_index + 4),
      rb_ary_entry(arrays, variable_index + 3),
      rb_ary_entry(arrays, variable_index + 2),
      rb_ary_entry(arrays, variable_index + 1),
      rb_ary_entry(arrays, variable_index + 0),
    };

    __m256 sums = _mm256_setzero_ps();
    __m256 means = _mm256_setzero_ps();
    __m256 mins = _mm256_set_ps(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
    __m256 maxes = _mm256_set_ps(FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN);
    __m256 variances = _mm256_setzero_ps();
    __m256 standard_deviations = _mm256_setzero_ps();
    float rows_d = (float)rows;
    __m256 lengths = _mm256_set_ps(rows_d, rows_d, rows_d, rows_d, rows_d, rows_d, rows_d, rows_d);

    for (int row_index = 0; row_index < rows; row_index++) {
      __m256 packed = pack_float32_m256(cols, row_index);
      sums = _mm256_add_ps(sums, packed);
      mins = _mm256_min_ps(mins, packed);
      maxes = _mm256_max_ps(maxes, packed);
    }
    means = _mm256_div_ps(sums, lengths);

    for (int row_index = 0; row_index < rows; row_index++) {
      __m256 packed = pack_float32_m256(cols, row_index);
      __m256 deviation = _mm256_sub_ps(packed, means);
      __m256 sqr_deviation = _mm256_mul_ps(deviation, deviation);
      variances = _mm256_add_ps(variances, _mm256_div_ps(sqr_deviation, lengths));
    }
    standard_deviations = _mm256_sqrt_ps(variances);

    for (int simd_slot_index = 0; simd_slot_index < simd_pack_size; simd_slot_index++) {
      VALUE h_result = rb_hash_new();

      float min = mm_get_index_float(mins, simd_slot_index);
      float max = mm_get_index_float(maxes, simd_slot_index);
      float mean = mm_get_index_float(means, simd_slot_index);
      float variance = mm_get_index_float(variances, simd_slot_index);
      float standard_deviation = mm_get_index_float(standard_deviations, simd_slot_index);

      rb_hash_aset(h_result, s_min, DBL2NUM(min));
      rb_hash_aset(h_result, s_max, DBL2NUM(max));
      rb_hash_aset(h_result, s_mean, DBL2NUM(mean));
      rb_hash_aset(h_result, s_variance, DBL2NUM(variance));
      rb_hash_aset(h_result, s_standard_deviation, DBL2NUM(standard_deviation));

      rb_ary_push(a_results, h_result);
    }
  }

  return a_results;
}
#endif

VALUE descriptive_statistics(VALUE self, VALUE arrays)
{
  Check_Type(arrays, T_ARRAY);

  int cols = rb_array_len(arrays);
  int rows = rb_array_len(rb_ary_entry(arrays, 0));

  VALUE a_results  = rb_ary_new();
  VALUE s_min = ID2SYM(rb_intern("min"));
  VALUE s_max = ID2SYM(rb_intern("max"));
  VALUE s_mean = ID2SYM(rb_intern("mean"));
  VALUE s_variance = ID2SYM(rb_intern("variance"));
  VALUE s_standard_deviation = ID2SYM(rb_intern("standard_deviation"));

  for (int variable_index = 0; variable_index < cols; variable_index++) {
    VALUE col = rb_ary_entry(arrays, variable_index);
    Check_Type(col, T_ARRAY);
    VALUE h_result = rb_hash_new();

    double sum = 0.0f;
    double min = FLT_MAX;
    double max = FLT_MIN;

    for (int i = 0; i < rows; i++) {
      VALUE value = rb_ary_entry(col, i);
      Check_Type(value, T_FLOAT);
      double valuef64 = RFLOAT_VALUE(value);

      sum += valuef64;
      if (valuef64 < min) min = valuef64;
      if (valuef64 > max) max = valuef64;
    }
    double mean = sum / rows;

    double variance = 0.0f;
    for (int i = 0; i < rows; i++) {
      VALUE value = rb_ary_entry(col, i);
      double valuef64 = NUM2DBL(value);

      double deviation = valuef64 - mean;
      double sqr_deviation = deviation * deviation;
      variance += (sqr_deviation / rows);
    }
    double standard_deviation = sqrt(variance);


    rb_hash_aset(h_result, s_min, DBL2NUM(min));
    rb_hash_aset(h_result, s_max, DBL2NUM(max));
    rb_hash_aset(h_result, s_mean, DBL2NUM(mean));
    rb_hash_aset(h_result, s_variance, DBL2NUM(variance));
    rb_hash_aset(h_result, s_standard_deviation, DBL2NUM(standard_deviation));

    rb_ary_push(a_results, h_result);
  }

  return a_results;
}

VALUE simd_disabled(VALUE self)
{
  return Qfalse;
}

void Init_fast_statistics(void)
{
  mFastStatistics = rb_define_module("FastStatistics");

#ifdef HAVE_XMMINTRIN_H
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics", descriptive_statistics_packed128_float64, 1);

  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_packed128_float32", descriptive_statistics_packed128_float32, 1);
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_packed128_float64", descriptive_statistics_packed128_float64, 1);

  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_unpacked", descriptive_statistics, 1);
  rb_define_singleton_method(mFastStatistics, "simd_enabled?", simd_enabled, 0);
#endif

#ifdef HAVE_IMMINTRIN_H
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics", descriptive_statistics_packed128_float64, 1);

  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_packed256_float32", descriptive_statistics_packed256_float32, 1);
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_packed256_float64", descriptive_statistics_packed256_float64, 1);

  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_unpacked", descriptive_statistics, 1);
  rb_define_singleton_method(mFastStatistics, "simd_enabled?", simd_enabled, 0);
#endif

#if !defined(HAVE_XMMINTRIN_H) && !defined(HAVE_IMMINTRIN_H)
  // No SIMD support:
  // Provide all "packed" and "unpacked" versions as the same algorithm
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics", descriptive_statistics, 1);
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_packed128_float32", descriptive_statistics, 1);
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_packed128_float64", descriptive_statistics, 1);
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_packed256_float32", descriptive_statistics, 1);
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_packed256_float64", descriptive_statistics, 1);
  rb_define_singleton_method(mFastStatistics, "descriptive_statistics_unpacked", descriptive_statistics, 1);
  rb_define_singleton_method(mFastStatistics, "simd_enabled?", simd_disabled, 0);
#endif
}
