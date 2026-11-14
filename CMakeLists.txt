cmake_minimum_required(VERSION 3.22)

# --- macOS : binaire universel (Apple Silicon + Intel), macOS 11 minimum ----------
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Version minimale de macOS")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS" FORCE)
endif()

project(Tresses VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(TRESSES_BUILD_PLUGIN "Compiler le plugin (VST3, AU, Standalone) avec JUCE" ON)
option(TRESSES_BUILD_TOOLS "Compiler les outils de test hors plugin (tools/)" OFF)

# --- Firmware ---------------------------------------------------------------------------
# Le plugin émule la puce du module (Source/emu) et exécute une image binaire de
# firmware. Celle qui est intégrée au plugin :
#   firmware/bin/braids-1.9.bin          Braids v1.9, extrait du .wav officiel de Mutable Instruments
#   firmware/bin/braids-1.9.factory.bin  ses réglages d'usine (calibration CAL faite)
set(TRESSES_PRODUCT_NAME "Tresses")

# Référence pour les tests : le même firmware recompilé pour l'ordinateur (Source/hw).
set(BRAIDS_FIRMWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/firmware/eurorack"
    CACHE PATH "Sources du firmware de référence (dossier contenant braids/ et stmlib/)")
if(TRESSES_BUILD_TOOLS)
  include(cmake/TressesFirmwareSlots.cmake)
  tresses_add_firmware_slots(tresses_module "${BRAIDS_FIRMWARE_DIR}" 2)
endif()

# --- Puce virtuelle : exécute une image binaire de firmware (Source/emu) ------------------
add_library(tresses_emu STATIC
  Source/emu/CortexM3.cpp Source/emu/BraidsMachine.cpp
  Source/emu/FirmwareFile.cpp Source/emu/FactorySetup.cpp)
target_include_directories(tresses_emu PUBLIC Source/emu)
set_target_properties(tresses_emu PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
  target_compile_options(tresses_emu PRIVATE -O3)
endif()

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
    BUNDLE_ID "com.labtechno.tresses"
    PLUGIN_MANUFACTURER_CODE Labt
    PLUGIN_CODE Tres
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

  juce_add_binary_data(TressesFirmware SOURCES
    firmware/bin/braids-1.9.bin
    firmware/bin/braids-1.9.factory.bin)
  set_target_properties(TressesFirmware PROPERTIES POSITION_INDEPENDENT_CODE ON)

  target_link_libraries(Tresses
    PRIVATE
      tresses_emu
      TressesFirmware
      juce::juce_audio_utils
    PUBLIC
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags
      juce::juce_recommended_warning_flags)
endif()

# --- Test du processeur du plugin (avec JUCE) ------------------------------------------------
if(TRESSES_BUILD_TOOLS AND TRESSES_BUILD_PLUGIN)
  juce_add_console_app(tresses_plugin_test)
  target_sources(tresses_plugin_test PRIVATE tools/plugin_test.cpp)
  target_include_directories(tresses_plugin_test PRIVATE Source Source/hw Source/engine)
  target_compile_definitions(tresses_plugin_test PRIVATE JucePlugin_Name="Tresses" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(tresses_plugin_test PRIVATE Tresses tresses_emu juce::juce_audio_utils)
endif()

# --- Outils de test (sans JUCE) -----------------------------------------------------------
if(TRESSES_BUILD_TOOLS)
  add_executable(tresses_module_test tools/module_test.cpp)
  target_link_libraries(tresses_module_test PRIVATE tresses_module)
  add_executable(tresses_knob_map tools/knob_map.cpp)
  target_link_libraries(tresses_knob_map PRIVATE tresses_module)
  add_executable(tresses_machine_test tools/machine_test.cpp)
  target_link_libraries(tresses_machine_test PRIVATE tresses_module tresses_emu)
  if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(tresses_module_test PRIVATE Threads::Threads)
  endif()
endif()
