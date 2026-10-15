if(NEUTRINO_BSTREAM_BUILD_DOCS)
    find_package(Doxygen 1.9.8 REQUIRED)
    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(bstream_docs
        COMMAND Doxygen::doxygen "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Building bstream API documentation"
        VERBATIM
    )
endif()

if(NEUTRINO_BSTREAM_BUILD_EXAMPLES OR NEUTRINO_BSTREAM_BUILD_TESTS)
    add_executable(bstream_usage_examples examples/usage.cc)
    target_link_libraries(bstream_usage_examples PRIVATE bstream)
    # Test builds apply the full project warning policy in test/CMakeLists.txt.
    if(NOT NEUTRINO_BSTREAM_BUILD_TESTS)
        neutrino_target_warnings(bstream_usage_examples PRIVATE)
        if(BSTREAM_WITH_BOOST_PFR)
            if(TARGET Boost::pfr)
                target_include_directories(bstream_usage_examples SYSTEM PRIVATE
                    $<TARGET_PROPERTY:Boost::pfr,INTERFACE_INCLUDE_DIRECTORIES>
                )
            elseif(DEFINED boost_pfr_SOURCE_DIR)
                target_include_directories(bstream_usage_examples SYSTEM PRIVATE "${boost_pfr_SOURCE_DIR}/include")
            endif()
        endif()
    endif()
endif()
