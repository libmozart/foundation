include_directories(.)

aux_source_directory(mozart++ SRCS)
aux_source_directory(src SRCS_IMPL)

add_library(mpp_foundation STATIC
        ${SRCS}
        ${SRCS_IMPL}
        )

target_link_libraries(mpp_foundation mpp_core)
set_target_properties(mpp_foundation PROPERTIES LINKER_LANGUAGE CXX)

add_executable(mpp_foundation-bench-any tests/benchmark-any.cpp)
target_link_libraries(mpp_foundation-bench-any mpp_core mpp_foundation)
set_property(TARGET mpp_foundation-bench-any PROPERTY CXX_STANDARD 17)

## test and benchmark targets here
file(GLOB_RECURSE CPP_SRC_LIST tests/test-*.cpp)
foreach(v ${CPP_SRC_LIST})
    string(REGEX MATCH "tests/.*" relative_path ${v})
    string(REGEX REPLACE "tests/" "" target_name ${relative_path})
    string(REGEX REPLACE ".cpp" "" target_name ${target_name})

    add_executable(mpp_foundation-${target_name} ${v})
    target_link_libraries(mpp_foundation-${target_name} mpp_foundation)
    add_test(mpp_foundation-${target_name} mpp_foundation-${target_name})
endforeach()
