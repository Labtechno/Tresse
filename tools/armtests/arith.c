// Test portable : le même code tourne sur l'ordinateur et sur le processeur émulé,
// les sorties doivent être identiques.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void emit(uint32_t v);
void put(char c);

static uint32_t seed = 0x12345678;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed ^ (seed >> 15); }
static uint32_t h = 2166136261u;
static void mix(uint32_t v) { for (int i = 0; i < 4; ++i) { h ^= (v >> (8 * i)) & 0xff; h *= 16777619u; } }
static void mix64(uint64_t v) { mix((uint32_t) v); mix((uint32_t) (v >> 32)); }

static const uint32_t edge[] = { 0, 1, 2, 3, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000, 0xffff, 0x10000,
  0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff, 0x55555555, 0xaaaaaaaa, 0x12345678 };
#define NEDGE (sizeof(edge) / sizeof(edge[0]))

struct Bits { unsigned a : 3; signed b : 7; unsigned c : 11; signed d : 5; unsigned e : 6; };

__attribute__((noinline)) static uint32_t sw(uint32_t x) {
  switch (x & 31) {
    case 0: return x + 1; case 1: return x * 3; case 2: return x ^ 0x55; case 3: return x >> 3;
    case 4: return ~x; case 5: return x << 5; case 6: return x - 77; case 7: return x | 0xf0f0;
    case 8: return x & 0x3c3c; case 9: return x * x; case 10: return x / 7; case 11: return x % 13;
    case 12: return (uint32_t) ((int32_t) x >> 4); case 13: return x + (x << 2); case 14: return 0u - x;
    case 15: return x ^ (x >> 16); case 20: return 1234567; case 25: return x * 0x01010101u;
    case 30: return (x >> 7) | (x << 25); default: return x;
  }
}

static int cmp_u32(const void* a, const void* b) {
  uint32_t x = *(const uint32_t*) a, y = *(const uint32_t*) b;
  return x < y ? -1 : x > y;
}

static uint32_t crc32(const uint8_t* p, size_t n) {
  uint32_t c = 0xffffffff;
  while (n--) { c ^= *p++; for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1))); }
  return ~c;
}

__attribute__((noinline)) static int32_t clip16(int32_t x) { return x > 32767 ? 32767 : (x < -32768 ? -32768 : x); }
__attribute__((noinline)) static uint32_t fib(uint32_t n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int main(void) {
  // 1. Arithmétique 32 bits sur valeurs limites.
  for (unsigned i = 0; i < NEDGE; ++i) for (unsigned j = 0; j < NEDGE; ++j) {
    uint32_t a = edge[i], b = edge[j]; int32_t sa = (int32_t) a, sb = (int32_t) b;
    mix(a + b); mix(a - b); mix(a * b); mix(a & b); mix(a | b); mix(a ^ b); mix(a & ~b); mix(a | ~b);
    mix(a < b); mix(sa < sb); mix(a <= b); mix(sa >= sb); mix(a == b);
    mix(b ? a / b : 7); mix(b ? a % b : 9);
    mix((sb && !(sa == INT32_MIN && sb == -1)) ? (uint32_t) (sa / sb) : 5);
    mix((sb && !(sa == INT32_MIN && sb == -1)) ? (uint32_t) (sa % sb) : 6);
    mix(a << (b & 31)); mix(a >> (b & 31)); mix((uint32_t) (sa >> (b & 31)));
    mix((a >> (b & 31)) | (a << ((32 - (b & 31)) & 31)));
    mix((uint32_t) __builtin_clz(a | 1)); mix(__builtin_bswap32(a)); mix((uint32_t) __builtin_popcount(a));
    mix((uint32_t) (int8_t) a); mix((uint32_t) (int16_t) a); mix((uint8_t) a + (uint16_t) b);
    mix((uint32_t) clip16(sa >> (b & 15)));
    mix64((uint64_t) a * b); mix64((uint64_t) ((int64_t) sa * sb));
    mix64(((uint64_t) a << 32 | b) + ((uint64_t) b << 32 | a));
    mix64(((uint64_t) a << 32 | b) - ((uint64_t) b << 29 | a));
  }
  emit(h);

  // 2. Arithmétique 64 bits (retenues, routines de division de la bibliothèque).
  for (int i = 0; i < 4000; ++i) {
    uint64_t a = (uint64_t) rnd() << 32 | rnd(), b = (uint64_t) rnd() << 32 | rnd();
    if (i & 1) { b >>= (rnd() & 63); }
    if (i & 2) { a >>= (rnd() & 63); }
    unsigned s = rnd() & 63;
    mix64(a + b); mix64(a - b); mix64(a * b); mix64(a << s); mix64(a >> s); mix64((uint64_t) ((int64_t) a >> s));
    mix64(b ? a / b : 1); mix64(b ? a % b : 2);
    mix64(b ? (uint64_t) ((int64_t) a / (int64_t) (b | 1)) : 3);
    mix(a < b); mix((int64_t) a < (int64_t) b);
    mix64(a + ((int64_t) (int32_t) (uint32_t) b * (int32_t) (uint32_t) a));
    mix((uint32_t) __builtin_clzll(a | 1));
  }
  emit(h);

  // 3. Champs de bits, tables de sauts, récursion, pile.
  struct Bits bits = { 0, 0, 0, 0, 0 };
  for (int i = 0; i < 3000; ++i) {
    uint32_t r = rnd();
    bits.a = r; bits.b = (int) (r >> 3); bits.c = r >> 10; bits.d = (int) (r >> 21); bits.e = r >> 26;
    mix(bits.a); mix((uint32_t) bits.b); mix(bits.c); mix((uint32_t) bits.d); mix(bits.e);
    bits.c += bits.a; bits.b -= bits.d;
    mix(sw(r)); mix(sw(r >> 5));
  }
  mix(fib(20));
  emit(h);

  // 4. Mémoire : memcpy/memmove/memset/strlen de la bibliothèque, tri, CRC.
  static uint8_t buf[4096]; static uint32_t words[512];
  for (int i = 0; i < 4096; ++i) buf[i] = (uint8_t) rnd();
  for (int i = 0; i < 300; ++i) {
    unsigned a = rnd() % 2000, b = rnd() % 2000, n = rnd() % 2000;
    unsigned m0 = rnd() % 4000, m1 = rnd(), m2 = rnd() % 90;   // ordre d'évaluation fixé
    memmove(buf + a, buf + b, n); memset(buf + m0, (int) m1, m2);
    memcpy(buf + 2048 + (a & 1023), buf + (b & 1023), n & 1023);
    mix(crc32(buf, 4096)); mix((uint32_t) memcmp(buf, buf + 1, 100) > 0);
  }
  for (int i = 0; i < 512; ++i) words[i] = rnd();
  qsort(words, 512, sizeof(uint32_t), cmp_u32);
  for (int i = 0; i < 512; ++i) mix(words[i]);
  buf[4095] = 0; mix((uint32_t) strlen((char*) buf));
  emit(h);

  // 5. printf de la bibliothèque (gros morceau de code compilé pour Thumb-2).
  char text[160];
  for (int i = 0; i < 200; ++i) {
    uint32_t v[10];
    for (int k = 0; k < 10; ++k) v[k] = rnd();
    int n = snprintf(text, sizeof(text), "%d %u %x %08lx %lld %c %s %5d|%-6u|%+d", (int) v[0], (unsigned) v[1],
                     (unsigned) v[2], (unsigned long) v[3], (long long) ((uint64_t) v[4] << 32 | v[5]),
                     'A' + (int) (v[6] % 26), "tresses", (int) (v[7] % 1000), (unsigned) (v[8] % 1000), (int) v[9] % 500);
    mix((uint32_t) n); mix(crc32((uint8_t*) text, (size_t) n));
    if (i == 0) { for (char* p = text; *p; ++p) put(*p); put('\n'); }
  }
  emit(h);

  // 6. malloc/free et pointeurs de fonctions.
  for (int i = 0; i < 100; ++i) {
    size_t n = 16 + rnd() % 700; uint8_t* p = malloc(n);
    if (!p) { emit(0xdead0000); return 1; }
    for (size_t k = 0; k < n; ++k) p[k] = (uint8_t) (k * 31 + i);
    mix(crc32(p, n)); free(p);
  }
  emit(h);
  return 0;
}
