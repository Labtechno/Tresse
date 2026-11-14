cmake_minimum_required(VERSION 3.22)

# --- macOS : binaire universel (Apple Silicon + Intel), macOS 11 minimum ----------
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Version minimale de macOS")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS" FORCE)
endif()

project(Tresses VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(TRESSES_BUILD_PLUGIN "Compiler le plugin (VST3, AU, Standalone) avec JUCE" ON)
option(TRESSES_BUILD_TOOLS "Compiler les outils de test hors plugin (tools/)" OFF)

# --- Firmware du module virtuel -------------------------------------------------------
# Le module virtuel exécute le firmware complet (braids.cc, ui.cc, settings.cc et
# le DSP) à partir de ses sources.
#   -DTRESSES_FIRMWARE=braids : firmware officiel Braids v1.9 (plugin "Tresses")
#   -DTRESSES_FIRMWARE=bees   : Bees-in-the-Trees v4.1 (plugin "Tresses Bees")
# Pour ton propre firmware : modifie les sources dans firmware/, ou passe
# -DBRAIDS_FIRMWARE_DIR=/chemin/vers/ton/fork (dossier contenant braids/ et stmlib/).
set(TRESSES_FIRMWARE "braids" CACHE STRING "Firmware du module : braids ou bees")
set_property(CACHE TRESSES_FIRMWARE PROPERTY STRINGS braids bees)

if(TRESSES_FIRMWARE STREQUAL "bees")
  set(default_firmware_dir "${CMAKE_CURRENT_SOURCE_DIR}/firmware/bees-in-the-trees")
  set(TRESSES_PRODUCT_NAME "Tresses Bees")
  set(TRESSES_PLUGIN_CODE TrBz)
  set(TRESSES_BUNDLE_ID "com.labtechno.tresses-bees")
  set(default_label "firmware Bees-in-the-Trees v4.1")
  set(default_credit "Firmware Bees-in-the-Trees © Tim Churches, d'après Braids © Emilie Gillet, licence MIT.")
else()
  set(default_firmware_dir "${CMAKE_CURRENT_SOURCE_DIR}/firmware/eurorack")
  set(TRESSES_PRODUCT_NAME "Tresses")
  set(TRESSES_PLUGIN_CODE Tres)
  set(TRESSES_BUNDLE_ID "com.labtechno.tresses")
  set(default_label "firmware Braids v1.9")
  set(default_credit "Firmware Braids v1.9 © Emilie Gillet, licence MIT.")
endif()

if(NOT BRAIDS_FIRMWARE_DIR)
  set(BRAIDS_FIRMWARE_DIR "${default_firmware_dir}")
endif()
if(NOT TRESSES_FIRMWARE_LABEL)
  set(TRESSES_FIRMWARE_LABEL "${default_label}")
endif()
if(NOT TRESSES_FIRMWARE_CREDIT)
  set(TRESSES_FIRMWARE_CREDIT "${default_credit}")
endif()
message(STATUS "Tresses : ${TRESSES_PRODUCT_NAME}, firmware dans ${BRAIDS_FIRMWARE_DIR}")

# Nombre maximal de Tresses ouverts en même temps dans un projet (une copie du
# firmware par instance).
set(TRESSES_MAX_INSTANCES 16 CACHE STRING "Nombre maximal d'instances simultanées")

include(cmake/TressesFirmwareSlots.cmake)
tresses_add_firmware_slots(tresses_module "${BRAIDS_FIRMWARE_DIR}" ${TRESSES_MAX_INSTANCES})

# --- Plugin ---------------------------------------------------------------------------
if(TRESSES_BUILD_PLUGIN)
  # Si un dossier JUCE/ existe à côté de ce fichier, il est utilisé ; sinon JUCE est
  # téléchargé automatiquement au premier "cmake" (connexion internet nécessaire).
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
  else()
    include(FetchContent)
    FetchContent_Declare(JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 8.0.15
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
  endif()

  juce_add_plugin(Tresses
    PRODUCT_NAME "${TRESSES_PRODUCT_NAME}"
    COMPANY_NAME "Labtechno"
    BUNDLE_ID "${TRESSES_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE Labt
    PLUGIN_CODE ${TRESSES_PLUGIN_CODE}
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE)

  target_sources(Tresses PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

  target_include_directories(Tresses PRIVATE Source Source/hw Source/engine)

  target_compile_definitions(Tresses PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

  # Textes affichés dans l'interface (nom du firmware, crédits).
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/TressesConfig.h"
       CONTENT "#pragma once\n#define TRESSES_FIRMWARE_LABEL \"@TRESSES_FIRMWARE_LABEL@\"\n#define TRESSES_FIRMWARE_CREDIT \"@TRESSES_FIRMWARE_CREDIT@\"\n"
       @ONLY)
  target_include_directories(Tresses PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

  target_link_libraries(Tresses
    PRIVATE
      tresses_module
      juce::juce_audio_utils
    PUBLIC
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags
      juce::juce_recommended_warning_flags)
endif()

# --- Outils de test (sans JUCE) -----------------------------------------------------------
if(TRESSES_BUILD_TOOLS)
  add_executable(tresses_module_test tools/module_test.cpp)
  target_link_libraries(tresses_module_test PRIVATE tresses_module)
  if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(tresses_module_test PRIVATE Threads::Threads)
  endif()
endif()
