add_executable(bstream_tests
    test_main.cc
    test_span_reader.cc
    test_stream_reader.cc
    test_bit_reader.cc
    test_bit_stream_reader.cc
    test_pfr.cc
    test_writer.cc
    test_byte_order.cc
    test_stream_writer.cc
    test_memory_stream.cc
    test_stream_copier.cc
    test_tee_stream.cc
    test_bit_writer.cc
    test_bit_stream_writer.cc
    test_bit_boundaries.cc
    test_serialization_boundaries.cc
    test_windows_macros.cc
    test_reader_byte_order.cc
)

target_link_libraries(bstream_tests PRIVATE
    bstream
    doctest::doctest
)

# Compile each installed header as the first and only include. This catches
# accidental dependencies on include order and on a particular standard library.
set(_bstream_headers
    bit_reader.hh bit_stream_reader.hh bit_stream_writer.hh bit_writer.hh
    bstream.hh byte_order.hh error.hh memory_stream.hh pfr_adapter.hh
    span_reader.hh span_writer.hh stream_copier.hh stream_reader.hh
    stream_writer.hh tee_stream.hh types.hh
    detail/bit_read_buffer.hh detail/endian_scope.hh detail/platform.hh
)
set(_bstream_header_sources)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/headers")
foreach(_header IN LISTS _bstream_headers)
    string(REPLACE "/" "_" _source "${_header}")
    set(_source "${CMAKE_CURRENT_BINARY_DIR}/headers/${_source}.cc")
    file(WRITE "${_source}" "#include <bstream/${_header}>\n")
    list(APPEND _bstream_header_sources "${_source}")
    if(WIN32)
        # Check both include orders without any project-wide NOMINMAX define.
        foreach(_order before after)
            set(_windows_source "${_source}.windows_${_order}.cc")
            if(_order STREQUAL "before")
                set(_includes "#include <windows.h>\n#include <bstream/${_header}>\n")
            else()
                set(_includes "#include <bstream/${_header}>\n#include <windows.h>\n")
            endif()
            file(WRITE "${_windows_source}" "${_includes}#if defined(min) || defined(max) || !defined(NOMINMAX)\n#error Windows macro configuration failed\n#endif\n")
            list(APPEND _bstream_header_sources "${_windows_source}")
        endforeach()
    endif()
endforeach()
add_library(bstream_header_checks OBJECT ${_bstream_header_sources})
target_link_libraries(bstream_header_checks PRIVATE bstream)

# Keep third-party diagnostics out of the project's warning-as-error policy.
if(BSTREAM_WITH_BOOST_PFR)
    foreach(_target bstream_tests bstream_header_checks bstream_usage_examples)
        if(TARGET Boost::pfr)
            target_include_directories(${_target} SYSTEM PRIVATE
                $<TARGET_PROPERTY:Boost::pfr,INTERFACE_INCLUDE_DIRECTORIES>
            )
        elseif(DEFINED boost_pfr_SOURCE_DIR)
            target_include_directories(${_target} SYSTEM PRIVATE "${boost_pfr_SOURCE_DIR}/include")
        endif()
    endforeach()
endif()

add_test(NAME bstream_tests COMMAND bstream_tests)
add_test(NAME bstream_usage_examples COMMAND bstream_usage_examples)

# Compile the fallback path even on machines with Boost.PFR installed. Keep
# this target free of bstream's optional dependency include paths/definitions.
add_executable(bstream_no_pfr_tests test_main.cc test_no_pfr.cc)
target_include_directories(bstream_no_pfr_tests PRIVATE ../include)
target_compile_features(bstream_no_pfr_tests PRIVATE cxx_std_20)
target_compile_definitions(bstream_no_pfr_tests PRIVATE BSTREAM_WITH_BOOST_PFR=0)
target_link_libraries(bstream_no_pfr_tests PRIVATE doctest::doctest)
add_test(NAME bstream_no_pfr_tests COMMAND bstream_no_pfr_tests)

if(NEUTRINO_BSTREAM_INSTALL)
    configure_file(install_smoke.cmake.in install_smoke.cmake @ONLY)
    add_test(NAME bstream_install_smoke
        COMMAND ${CMAKE_COMMAND} "-DTEST_CONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_BINARY_DIR}/install_smoke.cmake"
    )
endif()

# These flags check our headers and tests without imposing -Werror on consumers.
foreach(_target bstream_tests bstream_no_pfr_tests bstream_header_checks bstream_usage_examples)
    neutrino_target_warnings(${_target} PRIVATE)
    if(NOT MSVC AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(${_target} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wundef;-Wnull-dereference>
            $<$<CXX_COMPILER_ID:GNU>:-Wcast-align=strict;-Wformat-overflow=2;-Wformat-truncation=2;-Warray-bounds=2>
        )
    endif()
endforeach()
