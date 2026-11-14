// Pilote du test des drapeaux : voir gen_flags.py.
#include <stdint.h>
void emit(uint32_t v);
typedef uint32_t (*flag_fn)(uint32_t, uint32_t, uint32_t, uint32_t*);
extern const int flag_test_count;
extern const flag_fn flag_tests[];

static const uint32_t edge[] = { 0, 1, 2, 3, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000, 0xffff, 0x10000,
  0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff, 0x55555555, 0xaaaaaaaa, 0x12345678,
  31, 32, 33, 255, 256, 0x00800000, 0xff7fffff };
#define NEDGE (sizeof(edge) / sizeof(edge[0]))
static const uint32_t flagsIn[] = { 0, 0x20000000, 0x90000000, 0xf8000000, 0x40000000, 0x60000000 };

int main(void) {
  uint32_t total = 2166136261u;
  for (int t = 0; t < flag_test_count; ++t) {
    uint32_t h = 2166136261u, seed = 0xc0ffee + (uint32_t) t, out[2];
    for (unsigned n = 0; n < NEDGE * NEDGE * 6 + 600; ++n) {
      uint32_t a, b, fl;
      if (n < NEDGE * NEDGE * 6) { a = edge[n % NEDGE]; b = edge[(n / NEDGE) % NEDGE]; fl = flagsIn[n / (NEDGE * NEDGE)]; }
      else {
        seed = seed * 1664525u + 1013904223u; a = seed ^ (seed >> 13);
        seed = seed * 1664525u + 1013904223u; b = seed ^ (seed >> 11);
        if (n & 1) b &= 63;
        fl = seed & 0xf8000000;
      }
      uint32_t r = flag_tests[t](a, b, fl, out);
      h = (h ^ r) * 16777619u; h = (h ^ (out[0] & 0xf8000000)) * 16777619u; h = (h ^ out[1]) * 16777619u;
    }
    emit(h);
    total = (total ^ h) * 16777619u;
  }
  emit(total);
  return 0;
}
