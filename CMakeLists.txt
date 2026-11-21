cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# L'appareil : 1176 (ONIRIQUE F76) ou LA2A (ONIRIQUE L2A) : le même code, seul ce choix change
set(SOLO_MODEL "LA2A")

if(SOLO_MODEL STREQUAL "LA2A")
    set(SOLO_TARGET OniriqueL2A)
    set(SOLO_PRODUCT "ONIRIQUE L2A")
    set(SOLO_CODE Ola2)
    set(SOLO_IS_LA2A 1)
else()
    set(SOLO_TARGET OniriqueF76)
    set(SOLO_PRODUCT "ONIRIQUE F76")
    set(SOLO_CODE O176)
    set(SOLO_IS_LA2A 0)
endif()

project(${SOLO_TARGET} VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

option(SOLO_TESTS "Compiler le banc de mesure" ON)
option(SOLO_LICENCE "Brancher la licence ONIRIQUE" ON)
option(ONIRIQUE_UI_TEST "Build de captures et de démo" OFF)

set(SOLO_FONTS License/Fonts/Unbounded800.ttf License/Fonts/HankenGrotesk600.ttf
               License/Fonts/JetBrainsMono500.ttf License/Fonts/JetBrainsMono600.ttf)

set(SOLO_FORMATS VST3)
if(APPLE)
    list(APPEND SOLO_FORMATS AU)
endif()

juce_add_plugin(${SOLO_TARGET}
    COMPANY_NAME                Onirique
    PLUGIN_MANUFACTURER_CODE    Onir
    PLUGIN_CODE                 ${SOLO_CODE}
    FORMATS                     ${SOLO_FORMATS}
    AU_MAIN_TYPE                kAudioUnitType_Effect
    PRODUCT_NAME                "${SOLO_PRODUCT}"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(${SOLO_TARGET} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

juce_add_binary_data(SoloAssets SOURCES Resources/logo.png)
set_target_properties(SoloAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_include_directories(${SOLO_TARGET} PRIVATE Source ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(${SOLO_TARGET} PRIVATE SOLO_LA2A=${SOLO_IS_LA2A})

if(SOLO_LICENCE)
    include(License/OniriqueLicense.cmake)
    onirique_add_license(${SOLO_TARGET})
    target_compile_definitions(${SOLO_TARGET} PRIVATE SOLO_LICENCE=1)
else()
    juce_add_binary_data(SoloLicenseData HEADER_NAME OniriqueLicenseData.h NAMESPACE OniriqueLicenseData SOURCES ${SOLO_FONTS})
    set_target_properties(SoloLicenseData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    target_link_libraries(${SOLO_TARGET} PRIVATE SoloLicenseData)
    target_compile_definitions(${SOLO_TARGET} PRIVATE SOLO_LICENCE=0)
endif()

if(ONIRIQUE_UI_TEST)
    target_compile_definitions(${SOLO_TARGET} PRIVATE ONIRIQUE_UI_TEST=1)
endif()

target_compile_definitions(${SOLO_TARGET} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(${SOLO_TARGET} PRIVATE /utf-8)
endif()
if(APPLE)
    target_compile_definitions(${SOLO_TARGET} PUBLIC JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1)
endif()

target_link_libraries(${SOLO_TARGET}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        SoloAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Banc du moteur (console) : MIX à 0 et BYPASS = le son direct exactement retardé de la latence, compression qui agit,
# chaque suréchantillonnage
if(SOLO_TESTS)
    juce_add_console_app(SoloBench PRODUCT_NAME "SoloBench")
    target_sources(SoloBench PRIVATE Tests/SoloBench.cpp)
    target_include_directories(SoloBench PRIVATE Source)
    target_compile_definitions(SoloBench PRIVATE SOLO_LA2A=${SOLO_IS_LA2A} JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(MSVC)
        target_compile_options(SoloBench PRIVATE /utf-8)
    endif()
    target_link_libraries(SoloBench PRIVATE juce::juce_dsp juce::juce_audio_basics juce::juce_recommended_config_flags)
endif()
