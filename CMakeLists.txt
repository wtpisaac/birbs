# we get an error if we don't specify a version, i'll go for the default
cmake_minimum_required(
    VERSION
    3.31
)

# we need to give a project name or we get a warning
project(
    Birbs
)
# set C++ to 20 and cause an error if you're not using that.
# i really wonder why that isn't a default?
set(
    CMAKE_CXX_STANDARD
    20
)
set(
    CMAKE_CXX_STANDARD_REQUIRED
    ON
)
enable_testing()

option(
    ENABLE_ASAN
    "Enable address sanitizer"
    OFF
)

# get compile commands so clangd extension can read from them
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(
    vendored/glm
    EXCLUDE_FROM_ALL
)

# create an executable target which calls into the library code
# so we can next have a test target which also calls into the library
add_executable(
    birbs
    src/main.cpp
)

set_property(
    TARGET
    birbs
    PROPERTY
    CXX_STANDARD
    20
)
set_property(
    TARGET
    birbs
    PROPERTY
    CXX_STANDARD_REQUIRED
    ON
)

target_link_libraries(
    birbs
    PRIVATE
    glm::glm
)

if(ENABLE_ASAN)
    add_library(
        asan
        INTERFACE
    )

    if(MSVC)
    target_compile_options(
        asan
        INTERFACE
        /fsanitize=address
        /Zi
    )
    target_link_options(
        asan
        INTERFACE
        /INCREMENTAL:NO
    )
    else()
    target_link_options(
        asan
        INTERFACE
        -fsanitize=address
    )
    target_compile_options(
        asan
        INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer
    )
    endif()

    target_link_libraries(
        birbs
        PRIVATE
        asan
    )

    message(STATUS "Address Sanitizer (ASan) enabled")
endif()
