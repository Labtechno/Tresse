#!/usr/bin/env python3
"""Génère flags.S + flags_table.c : un petit sous-programme par instruction testée.

Chaque sous-programme reçoit (a, b, drapeaux, sortie), installe les drapeaux,
exécute l'instruction, puis rend le résultat, les drapeaux obtenus et un second
résultat. Le même binaire est exécuté par l'émulateur Tresses et par QEMU
(référence) : les sommes de contrôle doivent être identiques.
"""
ops = []
def add(*lines): ops.append(list(lines))

# --- Encodages 16 bits (modifient les drapeaux hors bloc IT) ---
for o in ["adds r0, r0, r1", "subs r0, r0, r1", "adcs r0, r1", "sbcs r0, r1", "ands r0, r1", "eors r0, r1",
          "orrs r0, r1", "bics r0, r1", "mvns r0, r1", "lsls r0, r1", "lsrs r0, r1", "asrs r0, r1",
          "rors r0, r1", "muls r0, r1, r0", "rsbs r0, r1, #0", "cmp r0, r1", "cmn r0, r1", "tst r0, r1",
          "adds r0, #200", "subs r0, #200", "adds r0, r1, #5", "subs r0, r1, #7", "cmp r0, #129",
          "movs r0, #77", "movs r0, r1", "add r0, r1", "mov r0, r1", "cmp r0, r8"]:
    add(o)
for n in [0, 1, 5, 16, 31]:
    add(f"lsls r0, r1, #{n}"); 
    if n: add(f"lsrs r0, r1, #{n}"); add(f"asrs r0, r1, #{n}")
add("lsrs r0, r1, #32"); add("asrs r0, r1, #32")
for o in ["sxtb r0, r1", "sxth r0, r1", "uxtb r0, r1", "uxth r0, r1", "rev r0, r1", "rev16 r0, r1", "revsh r0, r1"]:
    add(o)

# --- Encodages 32 bits ---
dp = ["and", "bic", "orr", "orn", "eor", "add", "adc", "sbc", "sub", "rsb"]
for o in dp:
    add(f"{o}s.w r0, r5, r1"); add(f"{o}.w r0, r5, r1")
    for sh in ["lsl #1", "lsl #13", "lsl #31", "lsr #1", "lsr #17", "lsr #32", "asr #1", "asr #9", "asr #32",
               "ror #1", "ror #21", "rrx"]:
        add(f"{o}s.w r0, r5, r1, {sh}")
for o in ["tst.w r5, r1", "teq.w r5, r1", "cmn.w r5, r1", "cmp.w r5, r1", "mvns.w r0, r1", "mvn.w r0, r1",
          "movs.w r0, r1", "tst.w r5, r1, lsl #7", "teq.w r5, r1, ror #3", "cmp.w r5, r1, asr #5",
          "cmn.w r5, r1, lsr #9", "mvns.w r0, r1, lsl #3", "mvns.w r0, r1, asr #32", "mvns.w r0, r1, rrx"]:
    add(o)
for o in ["lsl", "lsr", "asr", "ror"]:
    add(f"{o}s.w r0, r5, r1"); add(f"{o}.w r0, r5, r1")
    for n in [1, 8, 31]:
        add(f"{o}s.w r0, r5, #{n}")
consts = ["0xff", "0xab00ab", "0xcd00cd00", "0x01010101", "0xff000000", "0xff000", "0x80000000", "0x3fc", "0x1fe00", "0xa5"]
for c in consts:
    for o in dp:
        add(f"{o}s.w r0, r5, #{c}")
    add(f"movs.w r0, #{c}"); add(f"mvns.w r0, #{c}"); add(f"tst.w r5, #{c}"); add(f"teq.w r5, #{c}")
    add(f"cmp.w r5, #{c}"); add(f"cmn.w r5, #{c}"); add(f"add.w r0, r5, #{c}")
for o in ["addw r0, r5, #0xfff", "subw r0, r5, #0x801", "movw r0, #0xabcd", "movt r0, #0x1234",
          "mul r0, r5, r1", "mla r0, r5, r1, r5", "mls r0, r5, r1, r5", "umull r0, r3, r5, r1",
          "smull r0, r3, r5, r1", "umlal r0, r3, r5, r1", "smlal r0, r3, r5, r1", "sdiv r0, r5, r1",
          "udiv r0, r5, r1", "clz r0, r5", "rbit r0, r5", "rev.w r0, r5", "rev16.w r0, r5", "revsh.w r0, r5",
          "sxtb.w r0, r1", "sxth.w r0, r1", "uxtb.w r0, r1", "uxth.w r0, r1", "sxtb.w r0, r1, ror #8",
          "sxth.w r0, r1, ror #24", "uxtb.w r0, r1, ror #16", "uxth.w r0, r1, ror #8"]:
    add(o)
for n in [1, 2, 8, 16, 31, 32]:
    add(f"ssat r0, #{n}, r5")
for n in [0, 1, 8, 16, 31]:
    add(f"usat r0, #{n}, r5")
for o in ["ssat r0, #12, r5, lsl #4", "ssat r0, #9, r5, asr #7", "ssat r0, #16, r5, asr #31", "ssat r0, #20, r5, lsl #31",
          "usat r0, #12, r5, lsl #4", "usat r0, #9, r5, asr #7", "usat r0, #7, r5, asr #20"]:
    add(o)
for lsb, w in [(0, 1), (0, 32), (5, 7), (31, 1), (16, 16), (1, 30), (12, 9)]:
    add(f"ubfx r0, r5, #{lsb}, #{w}"); add(f"sbfx r0, r5, #{lsb}, #{w}")
    add(f"bfi r0, r1, #{lsb}, #{w}"); add(f"bfc r0, #{lsb}, #{w}")

# --- Exécution conditionnelle (blocs IT) ---
pairs = [("eq", "ne"), ("cs", "cc"), ("mi", "pl"), ("vs", "vc"), ("hi", "ls"), ("ge", "lt"), ("gt", "le")]
for a, b in pairs + [(y, x) for x, y in pairs]:
    add(f"ite {a}", f"mov{a} r0, #1", f"mov{b} r0, #2")
    add(f"it {a}", f"add{a} r0, r0, r1")               # 16 bits dans IT : drapeaux intacts
    add(f"itte {a}", f"lsl{a} r0, r0, #3", f"eor{a} r0, r1", f"sub{b} r0, r0, r1")
    add(f"itet {a}", f"adc{a} r0, r1", f"mvn{b} r0, r1", f"adds{a}.w r0, r5, r1")
    add(f"it {a}", f"cmp{a} r0, r1")
    add(f"b{a} 1f", "movs r0, #9", "1:")
    add(f"b{a}.w 1f", "mvns r0, r1", "1:")
add("cbz r0, 1f", "adds r0, #1", "1:"); add("cbnz r0, 1f", "adds r0, #1", "1:")

with open("flags.S", "w") as f:
    f.write(".syntax unified\n.cpu cortex-m3\n.thumb\n.text\n")
    for i, lines in enumerate(ops):
        f.write(f"\n.global flag_test_{i}\n.thumb_func\n.type flag_test_{i}, %function\nflag_test_{i}:\n")
        f.write("  push {r4, r5, r8, lr}\n  mov r4, r3\n  mov r5, r0\n  mov r3, r1\n  mov r8, r1\n  msr apsr_nzcvq, r2\n")
        for l in lines:
            f.write(f"  {l}\n" if not l.endswith(":") else f"{l}\n")
        f.write("  mrs r2, apsr\n  str r2, [r4]\n  str r3, [r4, #4]\n  pop {r4, r5, r8, pc}\n")
with open("flags_table.c", "w") as f:
    f.write("#include <stdint.h>\ntypedef uint32_t (*flag_fn)(uint32_t, uint32_t, uint32_t, uint32_t*);\n")
    for i in range(len(ops)):
        f.write(f"uint32_t flag_test_{i}(uint32_t, uint32_t, uint32_t, uint32_t*);\n")
    f.write(f"const int flag_test_count = {len(ops)};\nconst flag_fn flag_tests[] = {{\n")
    for i in range(len(ops)):
        f.write(f"  flag_test_{i},\n")
    f.write("};\n")
print(len(ops), "instructions testées")
