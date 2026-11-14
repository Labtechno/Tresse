#!/bin/bash
# Valide le processeur virtuel : chaque programme est compilé pour Cortex-M3, puis
# exécuté par l'émulateur Tresses. Ses sorties sont comparées à celles du même
# code compilé pour l'ordinateur et, si QEMU est installé, à celles de QEMU.
set -u
cd "$(dirname "$0")"
OUT=${OUT:-/tmp/tresses-armtests}
mkdir -p "$OUT"
CFLAGS="-mcpu=cortex-m3 -mthumb -ffunction-sections -fdata-sections -fno-common -Wall"
LDFLAGS="-nostartfiles -T link.ld -Wl,--gc-sections --specs=nosys.specs -Wl,--defsym=FLASH_ORIGIN=0"
fail=0

g++ -std=c++17 -O2 -o "$OUT/emu_run" ../emu_run.cpp ../../Source/emu/CortexM3.cpp || exit 1
python3 gen_flags.py > "$OUT/gen.log" && mv flags.S flags_table.c "$OUT/" || exit 1

check() {  # nom, fichier de référence, fichier obtenu
  if cmp -s "$2" "$3"; then printf "  %-34s OK (%s lignes)\n" "$1" "$(wc -l < "$3")"
  else printf "  %-34s DIFFERENT\n" "$1"; diff "$2" "$3" | head -6; fail=1; fi
}

run_qemu() {  # elf, sortie
  timeout 300 qemu-system-arm -M lm3s6965evb -nographic -monitor none -serial stdio \
      -semihosting -kernel "$1" > "$2" 2>/dev/null < /dev/null
}

build() {  # nom, optimisation, sources...
  local name=$1 opt=$2; shift 2
  arm-none-eabi-gcc $CFLAGS $opt "$@" startup.c $LDFLAGS -o "$OUT/$name.elf" 2> "$OUT/$name.build.log" || { echo "  $name : echec de compilation"; cat "$OUT/$name.build.log" | head; fail=1; return 1; }
  arm-none-eabi-objcopy -O binary "$OUT/$name.elf" "$OUT/$name.bin"
}

echo "== Arithmetique, bibliotheque C (reference : meme code compile pour l'ordinateur)"
gcc -O1 -w arith.c native_io.c -o "$OUT/arith_native" && "$OUT/arith_native" > "$OUT/arith.ref"
for opt in -O0 -O1 -O2 -Os -O3; do
  build "arith$opt" "$opt" arith.c || continue
  "$OUT/emu_run" "$OUT/arith$opt.bin" > "$OUT/arith$opt.out" 2> "$OUT/arith$opt.stat"
  check "arith $opt" "$OUT/arith.ref" "$OUT/arith$opt.out"
done

if command -v qemu-system-arm > /dev/null; then
  echo "== Drapeaux et encodages, instruction par instruction (reference : QEMU)"
  build flags -O1 flags.c "$OUT/flags.S" "$OUT/flags_table.c" && {
    "$OUT/emu_run" "$OUT/flags.bin" > "$OUT/flags.out" 2> "$OUT/flags.stat"
    run_qemu "$OUT/flags.elf" "$OUT/flags.ref"
    check "$(cat "$OUT/gen.log")" "$OUT/flags.ref" "$OUT/flags.out"
  }
  echo "== Exceptions et interruptions (reference : QEMU)"
  for opt in -O0 -O2; do
    build "irq$opt" "$opt" irq.c || continue
    "$OUT/emu_run" "$OUT/irq$opt.bin" > "$OUT/irq$opt.out" 2> "$OUT/irq$opt.stat"
    run_qemu "$OUT/irq$opt.elf" "$OUT/irq$opt.ref"
    check "irq $opt" "$OUT/irq$opt.ref" "$OUT/irq$opt.out"
  done
else
  echo "QEMU absent : tests des drapeaux et des exceptions non compares"
fi
echo "== Synthese Braids, 47 modeles (references : ordinateur et QEMU)"
FW=../../firmware/eurorack
DSP="$FW/braids/analog_oscillator.cc $FW/braids/digital_oscillator.cc $FW/braids/macro_oscillator.cc $FW/braids/quantizer.cc $FW/braids/resources.cc $FW/stmlib/utils/random.cc"
g++ -O2 -fwrapv -w -DTEST -I$FW braids_dsp.cc $DSP -x c native_io.c -o "$OUT/braids_native" && "$OUT/braids_native" > "$OUT/braids.ref"
# Mêmes options que le makefile du firmware (stmlib/makefile.inc).
arm-none-eabi-g++ $CFLAGS -O2 -fno-exceptions -fno-rtti -fshort-enums -fno-move-loop-invariants -fno-unwind-tables -Wno-all \
    -I$FW braids_dsp.cc $DSP -x c startup.c $LDFLAGS -o "$OUT/braids.elf" 2> "$OUT/braids.build.log" || { echo "  echec de compilation"; grep -m5 error "$OUT/braids.build.log"; fail=1; }
arm-none-eabi-objcopy -O binary "$OUT/braids.elf" "$OUT/braids.bin"
"$OUT/emu_run" "$OUT/braids.bin" > "$OUT/braids.out" 2> "$OUT/braids.stat"
check "emulateur = ordinateur" "$OUT/braids.ref" "$OUT/braids.out"
if command -v qemu-system-arm > /dev/null; then
  run_qemu "$OUT/braids.elf" "$OUT/braids.qemu"
  check "emulateur = QEMU" "$OUT/braids.qemu" "$OUT/braids.out"
fi
[ $fail = 0 ] && echo "TOUT EST CONFORME" || echo "DES ECARTS SUBSISTENT"
exit $fail
