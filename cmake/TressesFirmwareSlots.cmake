# Tresses — compilation du firmware Braids en "slots".
#
# Chaque fichier .cc du firmware est compilé une fois par slot, à l'intérieur d'un
# espace de noms propre (tresses_slot0, tresses_slot1...). Chaque slot a ainsi sa
# propre copie de toutes les variables globales du firmware : c'est ce qui permet
# d'avoir plusieurs Tresses en même temps dans un projet, comme plusieurs modules.
#
# Les drivers matériels (braids/drivers/*) et quelques en-têtes de stmlib/system
# sont remplacés par les versions virtuelles de Source/hw/shim, placées en tête
# du chemin d'inclusion. Le reste est compilé tel quel.

function(tresses_add_firmware_slots target firmware_dir num_slots)
  get_filename_component(tresses_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  file(GLOB firmware_cc CONFIGURE_DEPENDS "${firmware_dir}/braids/*.cc")
  list(SORT firmware_cc)
  list(APPEND firmware_cc
    "${firmware_dir}/stmlib/utils/random.cc"
    "${firmware_dir}/stmlib/system/system_clock.cc")

  # Famille de firmware : officiel (défaut) ou Bees-in-the-Trees 4.x.
  file(READ "${firmware_dir}/braids/braids.cc" braids_cc_content)
  set(firmware_bees 0)
  if(braids_cc_content MATCHES "envelope2")
    set(firmware_bees 1)
    message(STATUS "Tresses : firmware de la famille Bees-in-the-Trees détecté")
  else()
    message(STATUS "Tresses : firmware de la famille officielle Braids détecté")
  endif()

  set(slot_dir "${CMAKE_CURRENT_BINARY_DIR}/tresses_slots")
  set(sources)
  set(api_declarations "")
  set(api_table "")
  math(EXPR last_slot "${num_slots} - 1")

  foreach(slot RANGE 0 ${last_slot})
    foreach(src IN LISTS firmware_cc)
      get_filename_component(stem "${src}" NAME_WE)
      set(out "${slot_dir}/slot${slot}_${stem}.cpp")
      set(body "// Fichier généré par cmake/TressesFirmwareSlots.cmake : ne pas modifier.\n")
      string(APPEND body "#define TRESSES_SLOT_RENAME(name) tresses_slot${slot}_##name\n")
      string(APPEND body "#include \"slot_prefix.h\"\n")
      string(APPEND body "namespace tresses_slot${slot} {\n#include \"slot_compat.h\"\n#include \"${src}\"\n")
      if(stem STREQUAL "braids")
        string(APPEND body "#include \"slot_bridge.inc\"\n}  // namespace tresses_slot${slot}\n\n")
        string(APPEND body "extern const ::tresses::FirmwareSlotApi tresses_slot${slot}_api;\n")
        string(APPEND body "const ::tresses::FirmwareSlotApi tresses_slot${slot}_api = {\n")
        foreach(fn PowerOn Run EncoderInput ReadDisplay FlashVersion ReadFlash
                   SettingsSize ReadSettings WriteSettings SampleRate)
          string(APPEND body "  &tresses_slot${slot}::tresses_bridge::${fn},\n")
        endforeach()
        string(APPEND body "  tresses_slot${slot}::braids::chr_characters,\n};\n")
      else()
        string(APPEND body "}  // namespace tresses_slot${slot}\n")
      endif()
      file(CONFIGURE OUTPUT "${out}" CONTENT "${body}" @ONLY)
      list(APPEND sources "${out}")
    endforeach()
    string(APPEND api_declarations "extern const ::tresses::FirmwareSlotApi tresses_slot${slot}_api;\n")
    string(APPEND api_table "  &tresses_slot${slot}_api,\n")
  endforeach()

  set(table "// Fichier généré par cmake/TressesFirmwareSlots.cmake : ne pas modifier.\n")
  string(APPEND table "#include <atomic>\n#include \"FirmwareSlot.h\"\n\n${api_declarations}\n")
  string(APPEND table "namespace tresses {\nnamespace {\n")
  string(APPEND table "const FirmwareSlotApi* const kSlots[] = {\n${api_table}};\n")
  string(APPEND table "constexpr int kNumSlots = ${num_slots};\n")
  string(APPEND table "std::atomic<bool> gSlotUsed[kNumSlots];\n}  // namespace\n\n")
  string(APPEND table "int numFirmwareSlots() { return kNumSlots; }\n")
  string(APPEND table "const FirmwareSlotApi* firmwareSlot(int i) {\n  return (i >= 0 && i < kNumSlots) ? kSlots[i] : nullptr;\n}\n")
  string(APPEND table "int claimFirmwareSlot() {\n  for (int i = 0; i < kNumSlots; ++i) {\n    bool expected = false;\n")
  string(APPEND table "    if (gSlotUsed[i].compare_exchange_strong(expected, true)) return i;\n  }\n  return -1;\n}\n")
  string(APPEND table "void releaseFirmwareSlot(int i) {\n  if (i >= 0 && i < kNumSlots) gSlotUsed[i].store(false);\n}\n")
  string(APPEND table "}  // namespace tresses\n")
  file(CONFIGURE OUTPUT "${slot_dir}/slot_table.cpp" CONTENT "${table}" @ONLY)
  list(APPEND sources "${slot_dir}/slot_table.cpp")

  add_library(${target} STATIC ${sources})
  # Ordre important : les drivers virtuels masquent ceux du firmware.
  target_include_directories(${target} BEFORE PRIVATE
    "${tresses_root}/Source/hw/shim"
    "${firmware_dir}"
    "${tresses_root}/Source/hw")
  target_include_directories(${target} INTERFACE "${tresses_root}/Source/hw")
  # TEST : macro prévue par Mutable pour compiler le firmware sur ordinateur.
  target_compile_definitions(${target} PRIVATE TEST TRESSES_FIRMWARE_BEES=${firmware_bees})
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(NOT MSVC)
    # Arithmétique entière comme sur le Cortex-M3, pas d'avertissements du firmware.
    target_compile_options(${target} PRIVATE -O2 -w -fwrapv -fno-strict-aliasing)
  endif()
endfunction()
