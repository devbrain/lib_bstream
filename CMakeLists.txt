cmake_minimum_required(VERSION 3.20)

project(bstream
    VERSION 0.1.0
    DESCRIPTION "Modern C++20 zero-copy binary streaming and parsing library"
    LANGUAGES CXX
)

# ============================================================================
# Neutrino CMake Integration
# ============================================================================

include(FetchContent)

if(NOT DEFINED NEUTRINO_CMAKE_DIR)
    FetchContent_Declare(neutrino_cmake
        GIT_REPOSITORY https://github.com/devbrain/neutrino-cmake.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(neutrino_cmake)
    set(NEUTRINO_CMAKE_DIR "${neutrino_cmake_SOURCE_DIR}/cmake")
endif()

list(APPEND CMAKE_MODULE_PATH "${NEUTRINO_CMAKE_DIR}")
include(NeutrinoInit)

# ============================================================================
# C++ Standard
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
# Options
# ============================================================================

neutrino_define_library_options(bstream)
option(BSTREAM_WITH_FAILSAFE "Enable failsafe error and expected integration" ON)
option(BSTREAM_WITH_BOOST_PFR "Enable automatic aggregate struct reflection via Boost.PFR" ON)

# ============================================================================
# Dependencies
# ============================================================================

if(BSTREAM_WITH_FAILSAFE)
    include(${NEUTRINO_CMAKE_DIR}/deps/failsafe.cmake)
    neutrino_fetch_failsafe()
endif()

if(BSTREAM_WITH_BOOST_PFR)
    if(NOT TARGET Boost::pfr)
        FetchContent_Declare(boost_pfr
            GIT_REPOSITORY https://github.com/boostorg/pfr.git
            GIT_TAG b0bf18798c7037ca8a91a1cd2ad2e5798d8f6d46 # boost-1.83.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(boost_pfr)
    endif()
endif()

# ============================================================================
# Main Target: bstream (Header-only / Interface library)
# ============================================================================

include(GNUInstallDirs)

add_library(bstream INTERFACE)
add_library(neutrino::bstream ALIAS bstream)

target_include_directories(bstream INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(bstream INTERFACE cxx_std_20)
target_compile_definitions(bstream INTERFACE
    BSTREAM_WITH_BOOST_PFR=$<BOOL:${BSTREAM_WITH_BOOST_PFR}>
)

if(BSTREAM_WITH_FAILSAFE)
    if(TARGET neutrino::failsafe)
        target_link_libraries(bstream INTERFACE $<BUILD_INTERFACE:neutrino::failsafe>)
    else()
        target_link_libraries(bstream INTERFACE $<BUILD_INTERFACE:failsafe>)
    endif()
endif()

if(BSTREAM_WITH_BOOST_PFR)
    if(TARGET Boost::pfr)
        target_link_libraries(bstream INTERFACE $<BUILD_INTERFACE:Boost::pfr>)
    elseif(DEFINED boost_pfr_SOURCE_DIR)
        target_include_directories(bstream INTERFACE
            $<BUILD_INTERFACE:${boost_pfr_SOURCE_DIR}/include>
        )
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================

if(NEUTRINO_BSTREAM_INSTALL)
    include(CMakePackageConfigHelpers)
    set(_bstream_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/bstream")

    install(DIRECTORY include/bstream
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN "*.hh"
    )
    install(TARGETS bstream EXPORT bstreamTargets)
    install(EXPORT bstreamTargets
        NAMESPACE neutrino::
        DESTINATION "${_bstream_package_dir}"
    )
    configure_package_config_file(
        cmake/bstreamConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/bstreamConfig.cmake"
        INSTALL_DESTINATION "${_bstream_package_dir}"
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/bstreamConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/bstreamConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/bstreamConfigVersion.cmake"
        DESTINATION "${_bstream_package_dir}"
    )
endif()

# ============================================================================
# Documentation and Examples
# ============================================================================

if(NEUTRINO_BSTREAM_BUILD_DOCS OR NEUTRINO_BSTREAM_BUILD_EXAMPLES OR NEUTRINO_BSTREAM_BUILD_TESTS)
    add_subdirectory(docs)
endif()

# ============================================================================
# Tests
# ============================================================================

if(NEUTRINO_BSTREAM_BUILD_TESTS)
    include(${NEUTRINO_CMAKE_DIR}/deps/doctest.cmake)
    neutrino_fetch_doctest()

    enable_testing()
    add_subdirectory(test)
endif()

# ============================================================================
# Summary
# ============================================================================

neutrino_is_top_level(_is_top_level)
if(_is_top_level)
    neutrino_print_options(bstream)
endif()
