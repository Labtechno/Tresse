// Tresses — banc d'essai du processeur virtuel.
//
//   emu_run <image.bin> [adresse de chargement en hexa] [limite en millions de cycles]
//
// Charge une image brute en flash, exécute, et recopie sur la sortie standard ce
// que le programme écrit sur le port série de test (0x4000C000). Une écriture à
// 0x4000C004 termine le programme. Les statistiques vont sur la sortie d'erreur.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../Source/emu/CortexM3.h"

using namespace tresses::emu;

struct TestBus : Bus {
  CortexM3* cpu = nullptr;
  int exitCode = -1;
  uint32_t mark = 0;
  uint64_t markCycles = 0;
  uint32_t read(uint32_t, int) override { return 0; }
  void write(uint32_t a, uint32_t v, int) override {
    if (a == 0x4000c000u) std::fputc(static_cast<int>(v & 0xff), stdout);
    else if (a == 0x4000c004u) { exitCode = static_cast<int>(v); cpu->halt("exit"); }
    else if (a == 0x4000c010u) {
      // Repères de mesure : cycles consommés entre un repère non nul et le repère 0.
      if (v) { mark = v; markCycles = cpu->cycles(); }
      else std::fprintf(stderr, "[mesure] %u %llu\n", mark, static_cast<unsigned long long>(cpu->cycles() - markCycles));
    }
  }
  void onDeadline(uint64_t) override {}
};

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: emu_run image.bin [base] [Mcycles]\n"); return 2; }
  const uint32_t base = argc > 2 ? static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 16)) : 0;
  const uint64_t limit = (argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 20000) * 1000000ull;

  TestBus bus;
  CortexM3 cpu(256 * 1024, 64 * 1024, &bus);
  bus.cpu = &cpu;
  FILE* f = std::fopen(argv[1], "rb");
  if (!f) { std::perror(argv[1]); return 2; }
  const uint32_t offset = base >= CortexM3::kFlashBase ? base - CortexM3::kFlashBase : base;
  const size_t n = std::fread(cpu.flash() + offset, 1, cpu.flashSize() - offset, f);
  std::fclose(f);

  cpu.reset();
  const auto t0 = std::chrono::steady_clock::now();
  cpu.run(limit);
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::fflush(stdout);
  std::fprintf(stderr,
               "[emu] %zu octets, %llu instructions, %llu cycles, %.2f s (%.0f M instr/s, %.1fx un STM32 a 72 MHz), fin: %s",
               n, static_cast<unsigned long long>(cpu.instructions()),
               static_cast<unsigned long long>(cpu.cycles()), seconds,
               cpu.instructions() / seconds / 1e6, cpu.cycles() / seconds / 72e6, cpu.haltReason());
  if (bus.exitCode < 0) std::fprintf(stderr, " (pc=%08x, faute a %08x)", cpu.pc(), cpu.faultPc());
  std::fprintf(stderr, "\n");
  return bus.exitCode < 0 ? 3 : bus.exitCode;
}
