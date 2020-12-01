#include<float.h>
#include<ruby.h>

VALUE descriptive_statistics(VALUE self, VALUE arrays);

#ifdef HAVE_XMMINTRIN_H
#include<xmmintrin.h>

VALUE descriptive_statistics_packed_float32(VALUE self, VALUE arrays);
VALUE descriptive_statistics_packed_float64(VALUE self, VALUE arrays);
#endif
