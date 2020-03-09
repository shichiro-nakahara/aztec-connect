# copyright 2019 Spilsbury Holdings
#
# usage: barretenberg_module(module_name [dependencies ...])

function(barretenberg_module MODULE_NAME)
    file(GLOB_RECURSE SOURCE_FILES *.cpp)
    file(GLOB_RECURSE HEADER_FILES *.hpp)
    list(FILTER SOURCE_FILES EXCLUDE REGEX ".*\.test.cpp$")

    if(SOURCE_FILES)
        add_library(
            ${MODULE_NAME}_objects
            OBJECT
            ${SOURCE_FILES}
        )

        add_library(
            ${MODULE_NAME} STATIC
            $<TARGET_OBJECTS:${MODULE_NAME}_objects>
        )

        target_link_libraries(
            ${MODULE_NAME}
            INTERFACE
            ${ARGN}
        )

        if(WASM)
            add_executable(
                ${MODULE_NAME}.wasm
                $<TARGET_OBJECTS:${MODULE_NAME}_objects>
            )

            target_link_options(
                ${MODULE_NAME}.wasm
                PRIVATE
                -nostartfiles -Wl,--no-entry -Wl,--export-dynamic -Wl,--import-memory
            )
        endif()

        set(MODULE_LINK_NAME ${MODULE_NAME})
    endif()

    file(GLOB_RECURSE TEST_SOURCE_FILES *.test.cpp)
    if(TESTING AND TEST_SOURCE_FILES)
        # We have to get a bit complicated here, due to the fact CMake will not parallelise the building of object files
        # between dependent targets, due to the potential of post-build code generation steps etc.
        # To work around this, we create an "object library" containing the test object files, that only has a
        # dependency on gtest (to pull in the gtest include directory). Then we declare an executable that is to be
        # built from these object files. This executable will only be linked once it's dependencies are complete, but
        # that's pretty fast.

        add_library(
            ${MODULE_NAME}_test_objects
            OBJECT
            ${TEST_SOURCE_FILES}
        )

        target_link_libraries(
            ${MODULE_NAME}_test_objects
            PRIVATE
            gtest
        )

        add_executable(
            ${MODULE_NAME}_tests
            $<TARGET_OBJECTS:${MODULE_NAME}_test_objects>
        )

        if(WASM)
            target_link_options(
                ${MODULE_NAME}_tests
                PRIVATE
                -Wl,-z,stack-size=8388608
            )
        endif()

        target_link_libraries(
            ${MODULE_NAME}_tests
            PRIVATE
            ${MODULE_LINK_NAME}
            gtest
            gtest_main
        )

        gtest_discover_tests(${MODULE_NAME}_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    endif()
endfunction()