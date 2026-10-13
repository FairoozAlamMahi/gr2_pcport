cmake_minimum_required(VERSION 3.25)
project(gr2_pc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Clang compiler flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
    if (MSVC)
        add_compile_options(-fms-extensions -fms-compatibility)
        add_link_options("-Wl,/HIGHENTROPYVA:NO" "-Wl,/LARGEADDRESSAWARE")
    else()
        add_link_options("-Wl,--disable-high-entropy-va" "-static")
    endif()
endif()

include_directories(src)

set(SOURCES
    src/main.cpp
    src/memory/vm.cpp
    src/loader/elf_loader.cpp
    src/kernel/libkernel.cpp
    src/pad/pad_handler.cpp
    src/video/video_out.cpp
    src/audio/audio_out.cpp
    src/gpu/gnm_driver.cpp
)

set(HEADERS
    src/common/types.h
    src/common/logger.h
    src/memory/vm.h
    src/loader/elf_loader.h
    src/kernel/libkernel.h
    src/pad/pad_handler.h
    src/video/video_out.h
    src/audio/audio_out.h
    src/gpu/gnm_driver.h
)

add_executable(gr2_pc ${SOURCES} ${HEADERS})

target_link_libraries(gr2_pc PRIVATE
    xinput
    onecore
)

set_target_properties(gr2_pc PROPERTIES
    OUTPUT_NAME "gr2_pc"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
