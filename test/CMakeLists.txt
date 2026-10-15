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
)

target_link_libraries(bstream_tests PRIVATE
    bstream
    doctest::doctest
)

add_test(NAME bstream_tests COMMAND bstream_tests)
