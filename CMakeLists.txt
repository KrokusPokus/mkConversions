cmake_minimum_required(VERSION 3.19)
project(mkConversions VERSION 1.0.0 LANGUAGES CXX)

# Enforce the C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- 1. BUILD TYPE ---
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(GNUInstallDirs)

include(CheckIPOSupported)
check_ipo_supported(RESULT result)
if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_compile_options(
    -Wall -Wextra
    $<$<NOT:$<CONFIG:Debug>>:-O2>
)

# --- 2. QT & STANDARD SETUP ---
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(mkConversions
    src/main.cpp

    src/Conversions.h src/Conversions.cpp
    src/ConversionDlg.h src/ConversionDlg.cpp
    src/Converter.h src/Converter.cpp
    src/ElidedLabel.h src/ElidedLabel.cpp

    res/resources.qrc
)

qt_add_translations(mkConversions
    TS_FILES
        i18n/mkConversions_de.ts
        i18n/mkConversions_en.ts
    RESOURCE_PREFIX
        "/i18n"
)

target_compile_definitions(mkConversions PRIVATE
    MKCONVERSIONS_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_BYTEARRAY
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(mkConversions PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mkConversions PRIVATE
    Qt6::Core
    Qt6::Widgets
)

# Speeds up startup on Linux by ignoring unused shared libraries
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(mkConversions PRIVATE -Wl,--as-needed)
endif()

set_target_properties(mkConversions PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS mkConversions
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES res/mkConversions.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

if(LINUX)
    add_custom_command(TARGET mkConversions POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:mkConversions>
            $ENV{HOME}/.local/bin/mkConversions

        COMMENT "Copying mkConversions to ~/.local/bin/"
    )
endif()
