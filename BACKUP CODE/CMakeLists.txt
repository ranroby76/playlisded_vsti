cmake_minimum_required(VERSION 3.22)

# --- PROJECT SETUP ---
project(Playlisted LANGUAGES C CXX VERSION 2.0.0)

# --- CCACHE ---
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using CCache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# --- PATHS ---
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(ASSETS_DIR   "${PROJECT_ROOT}/assets")
set(SRC_DIR      "${PROJECT_ROOT}/src")

# --- PLATFORM DETECTION ---
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IOS TRUE)
    message(STATUS "Building for iOS")
endif()

# --- JUCE SETUP (MUST BE FIRST) ---
if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "D:/Workspace/JUCE") 
endif()

if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE_Build")
else()
    message(FATAL_ERROR "JUCE directory not found at ${JUCE_DIR}!")
endif()

# --- CLAP EXTENSIONS SETUP (Desktop only) ---
if(NOT IOS)
    if(NOT DEFINED CLAP_EXTENSIONS_DIR)
        set(CLAP_EXTENSIONS_DIR "${PROJECT_ROOT}/libs/CLAP_Extensions") 
    endif()

    get_filename_component(CLAP_EXTENSIONS_ABS "${CLAP_EXTENSIONS_DIR}" ABSOLUTE)
    message(STATUS "CLAP Extension Path: ${CLAP_EXTENSIONS_ABS}")

    add_subdirectory("${CLAP_EXTENSIONS_ABS}" "${CMAKE_BINARY_DIR}/CLAP_Extensions_Build")
endif()

# ==============================================================================
# 1. PLAYLISTED ENGINE (Desktop only)
# ==============================================================================
if(NOT IOS)
    set(SHARED_SOURCES ${SRC_DIR}/AppLogger.h ${SRC_DIR}/IPC/SharedMemoryManager.h)
    set(ENGINE_SOURCES ${SHARED_SOURCES} ${SRC_DIR}/EngineMain.cpp)

    if(WIN32)
        list(APPEND ENGINE_SOURCES ${SRC_DIR}/engine/VLCMediaPlayer_Desktop.cpp ${SRC_DIR}/engine/VLCMediaPlayer_Desktop.h)
    elseif(APPLE)
        list(APPEND ENGINE_SOURCES ${SRC_DIR}/engine/NativeMediaPlayer_Apple.mm ${SRC_DIR}/engine/NativeMediaPlayer_Apple.h)
    endif()

    juce_add_gui_app(PlaylistedEngine PRODUCT_NAME "PlaylistedEngine" VERSION "2.0.0" ICON_BIG "${ASSETS_DIR}/logo.png")
    target_sources(PlaylistedEngine PRIVATE ${ENGINE_SOURCES})

    if(WIN32)
        add_definitions(-DJUCE_WINDOWS=1)
        set(VLC_DIR "${PROJECT_ROOT}/vlc-3.0.21")
        target_include_directories(PlaylistedEngine PRIVATE ${VLC_DIR}/sdk/include)
        target_link_libraries(PlaylistedEngine PRIVATE "${VLC_DIR}/sdk/lib/libvlc.lib" "${VLC_DIR}/sdk/lib/libvlccore.lib" winmm ws2_32 gdi32 ole32 uuid comdlg32 shlwapi)
        add_custom_command(TARGET PlaylistedEngine POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VLC_DIR}/libvlc.dll" "$<TARGET_FILE_DIR:PlaylistedEngine>")
        add_custom_command(TARGET PlaylistedEngine POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VLC_DIR}/libvlccore.dll" "$<TARGET_FILE_DIR:PlaylistedEngine>")
        add_custom_command(TARGET PlaylistedEngine POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${VLC_DIR}/plugins" "$<TARGET_FILE_DIR:PlaylistedEngine>/plugins")
    elseif(APPLE)
        add_definitions(-DJUCE_MAC=1)
        target_link_libraries(PlaylistedEngine PRIVATE "-framework Cocoa" "-framework CoreAudio" "-framework CoreMIDI" "-framework AudioToolbox" "-framework IOKit" "-framework Accelerate" "-framework QuartzCore" "-framework WebKit" "-framework Foundation" "-framework AVFoundation" "-framework CoreMedia" "-framework CoreVideo")
    endif()

    target_link_libraries(PlaylistedEngine PRIVATE juce::juce_core juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats)
endif()

# ==============================================================================
# 2. PLAYLISTED PLUGIN
# ==============================================================================
set(PLUGIN_SOURCES ${SRC_DIR}/AudioEngine.cpp ${SRC_DIR}/AudioEngine.h ${SRC_DIR}/IOSettingsManager.cpp ${SRC_DIR}/IOSettingsManager.h ${SRC_DIR}/RegistrationManager.cpp ${SRC_DIR}/RegistrationManager.h ${SRC_DIR}/PluginProcessor.cpp ${SRC_DIR}/PluginProcessor.h ${SRC_DIR}/PluginEditor.cpp ${SRC_DIR}/PluginEditor.h ${SRC_DIR}/engine/VideoSurfaceComponent.cpp ${SRC_DIR}/engine/VideoSurfaceComponent.h ${SRC_DIR}/UI/MainComponent.cpp ${SRC_DIR}/UI/MainComponent.h ${SRC_DIR}/UI/HeaderBar.cpp ${SRC_DIR}/UI/HeaderBar.h ${SRC_DIR}/UI/RegistrationComponent.h ${SRC_DIR}/UI/MediaPage.cpp ${SRC_DIR}/UI/MediaPage.h ${SRC_DIR}/UI/PlaylistComponent.cpp ${SRC_DIR}/UI/PlaylistComponent.h ${SRC_DIR}/UI/TrackBannerComponent.cpp ${SRC_DIR}/UI/TrackBannerComponent.h ${SRC_DIR}/UI/PlaylistDataStructures.h ${SRC_DIR}/UI/DebugConsole.h ${SRC_DIR}/UI/ManualComponent.h ${SRC_DIR}/UI/LongPressDetector.h ${SRC_DIR}/UI/StyledSlider.h ${SRC_DIR}/UI/SignalLed.h)

# Add desktop-specific sources
if(NOT IOS)
    list(APPEND PLUGIN_SOURCES ${SRC_DIR}/AppLogger.h ${SRC_DIR}/IPC/SharedMemoryManager.h "${PROJECT_ROOT}/resources.rc")
else()
    # iOS-specific sources (AVFoundation player instead of Engine)
    list(APPEND PLUGIN_SOURCES ${SRC_DIR}/engine/NativeMediaPlayer_Apple.mm ${SRC_DIR}/engine/NativeMediaPlayer_Apple.h)
endif()

# Determine plugin formats
if(IOS)
    set(PLUGIN_FORMATS AUv3 Standalone)
else()
    set(PLUGIN_FORMATS VST3 AU CLAP)
endif()

# Binary Data (Linked globally to avoid missing header issues)
juce_add_binary_data(OnStageAssets SOURCES "${ASSETS_DIR}/logo.png" "${ASSETS_DIR}/playlisted2.png" "${ASSETS_DIR}/ir.wav" "${ASSETS_DIR}/license.mid")

juce_add_plugin(Playlisted 
    COMPANY_NAME "Fanan" 
    IS_SYNTH TRUE 
    NEEDS_MIDI_INPUT TRUE 
    NEEDS_MIDI_OUTPUT FALSE 
    IS_MIDI_EFFECT FALSE 
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE 
    COPY_PLUGIN_AFTER_BUILD TRUE 
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Playlisted2" 
    VERSION "2.0.0"
    BUNDLE_ID "com.fanan.playlisted2"
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight)

target_sources(Playlisted PRIVATE ${PLUGIN_SOURCES})
target_compile_definitions(Playlisted PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

# Link Assets Globally
target_link_libraries(Playlisted PRIVATE OnStageAssets)

# iOS-specific settings
if(IOS)
    foreach(TARGET_NAME Playlisted_AUv3 Playlisted_Standalone)
        set_target_properties(${TARGET_NAME} PROPERTIES
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "12.0"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Distribution"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${IOS_TEAM_ID}"
        )
        
        target_link_libraries(${TARGET_NAME} PRIVATE 
            "-framework UIKit"
            "-framework AVFoundation"
            "-framework CoreAudio"
            "-framework CoreMIDI"
            "-framework AudioToolbox"
            "-framework Accelerate"
            "-framework QuartzCore"
            "-framework CoreMedia"
            "-framework CoreVideo"
        )
    endforeach()

    # AUv3: Force MANUAL signing
    if(IOS_AUV3_PROFILE_ID)
        set_target_properties(Playlisted_AUv3 PROPERTIES
            XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${IOS_AUV3_PROFILE_ID}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
        )
    endif()

    # Standalone App: Manual if profile exists, else Automatic
    if(IOS_APP_PROFILE_ID)
        set_target_properties(Playlisted_Standalone PROPERTIES
            XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${IOS_APP_PROFILE_ID}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
        )
    else()
        message(STATUS "No App Profile ID provided. Defaulting Standalone to Automatic Signing.")
        set_target_properties(Playlisted_Standalone PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        )
    endif()
    
    # Copy assets
    add_custom_command(TARGET Playlisted_Standalone POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_DIR}" "$<TARGET_FILE_DIR:Playlisted_Standalone>/assets")
        
    add_custom_command(TARGET Playlisted_AUv3 POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_DIR}" "$<TARGET_FILE_DIR:Playlisted_AUv3>/../Resources/assets")
else()
    # Enable CLAP for desktop
    target_link_libraries(Playlisted PRIVATE clap_juce_extensions)
    clap_juce_extensions_plugin(TARGET Playlisted
                                 CLAP_ID "com.fanan.playlisted2"
                                 CLAP_FEATURES instrument)
    
    add_dependencies(Playlisted PlaylistedEngine)
endif()

if(WIN32)
    target_link_libraries(Playlisted PRIVATE winmm ws2_32 gdi32 ole32 uuid comdlg32 shlwapi)
    macro(copy_engine_to_target target_name)
        if(TARGET ${target_name})
            add_custom_command(TARGET ${target_name} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:PlaylistedEngine>" "$<TARGET_FILE_DIR:${target_name}>/PlaylistedEngine.exe")
            add_custom_command(TARGET ${target_name} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VLC_DIR}/libvlc.dll" "$<TARGET_FILE_DIR:${target_name}>/")
            add_custom_command(TARGET ${target_name} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VLC_DIR}/libvlccore.dll" "$<TARGET_FILE_DIR:${target_name}>/")
            add_custom_command(TARGET ${target_name} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${VLC_DIR}/plugins" "$<TARGET_FILE_DIR:${target_name}>/plugins")
        endif()
    endmacro()
    copy_engine_to_target(Playlisted_VST3)
    copy_engine_to_target(Playlisted_CLAP)
elseif(APPLE AND NOT IOS)
    target_link_libraries(Playlisted PRIVATE "-framework Cocoa" "-framework Foundation")
    add_custom_command(TARGET Playlisted POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_DIR}" "$<TARGET_FILE_DIR:Playlisted>/../Resources/assets")
    
    macro(copy_mac_engine target_name)
        if(TARGET ${target_name})
            add_custom_command(TARGET ${target_name} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:PlaylistedEngine>" "$<TARGET_FILE_DIR:${target_name}>/../Resources/PlaylistedEngine")
        endif()
    endmacro()
    
    copy_mac_engine(Playlisted_VST3)
    copy_mac_engine(Playlisted_CLAP)
    copy_mac_engine(Playlisted_AU)
endif()

target_include_directories(Playlisted PRIVATE ${PROJECT_ROOT} ${SRC_DIR} ${SRC_DIR}/engine ${SRC_DIR}/UI)
target_link_libraries(Playlisted PRIVATE juce::juce_core juce::juce_events juce::juce_data_structures juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_audio_plugin_client)