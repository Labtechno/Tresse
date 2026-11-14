// Mêmes sorties que startup.c, pour la version compilée sur l'ordinateur.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
void put(char c) { fputc(c, stdout); }
void emit(uint32_t v) { printf("%08x\n", v); }
void finish(int code) { exit(code); }
