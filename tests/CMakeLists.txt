

set(UT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extern/ut)
set(UT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/extern/ut)

add_subdirectory(
    ${UT_SOURCE_DIR}
    ${UT_BINARY_DIR}
    EXCLUDE_FROM_ALL
)

add_library(test_options INTERFACE)

target_compile_features(test_options INTERFACE cxx_std_26)

target_compile_options(
    test_options
    INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-O0>
    $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>
)

# 2. Find all test files matching *.test.cpp recursively
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.test.cpp")

# 1. Collect all generated test target names into a list
set(ALL_TEST_TARGETS "")

# 3. Create an executable target and test for each source file found
foreach(TEST_SRC IN LISTS TEST_SOURCES)
    # Get the base filename without extension (e.g., "math.test.cpp" -> "math.test")
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    
    # Optional: Clean up name (e.g., turn "math.test" into "test_math")
    string(REGEX REPLACE "\\.test$" "" CLEAN_NAME ${TEST_NAME})
    set(TARGET_NAME "test_${CLEAN_NAME}")

    # Create the test executable
    add_executable(${TARGET_NAME} ${TEST_SRC})

    # Link shared project flags/options and boost-ext/ut
    target_link_libraries(
        ${TARGET_NAME}
        PRIVATE
        spc_options
        test_options
    )

    target_include_directories(
        ${TARGET_NAME}
        BEFORE
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Register the individual binary with CTest
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})

    # Append to our list
    list(APPEND ALL_TEST_TARGETS ${TARGET_NAME})
endforeach()

if(ALL_TEST_TARGETS)
    add_custom_target(tests DEPENDS ${ALL_TEST_TARGETS})
endif()