cmake_minimum_required(VERSION 3.20)
project(bstream_install_consumer LANGUAGES CXX)

option(EXPECT_PFR "Expect the installed package to enable reflection" OFF)
option(EXPECT_FAILSAFE "Expect the installed package to enable failsafe" OFF)
if(DEPENDENCY_SETUP)
    include("${DEPENDENCY_SETUP}")
endif()

find_package(bstream ${EXPECTED_VERSION} CONFIG REQUIRED)
# Repeated discovery must not redefine the target or drop its dependencies.
find_package(bstream ${EXPECTED_VERSION} CONFIG REQUIRED)
add_executable(consumer main.cc)
target_link_libraries(consumer PRIVATE neutrino::bstream)
target_compile_definitions(consumer PRIVATE
    EXPECT_PFR=$<BOOL:${EXPECT_PFR}> EXPECT_FAILSAFE=$<BOOL:${EXPECT_FAILSAFE}>)
enable_testing()
add_test(NAME installed_bstream_consumer COMMAND consumer)
