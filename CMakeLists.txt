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

include_directories(
    src
    third_party/Vulkan-Headers/include
)

set(SOURCES
    src/main.cpp
    src/common/crash_handler.cpp
    src/memory/vm.cpp
    src/loader/elf_loader.cpp
    src/loader/prx_module.cpp
    src/loader/nid_table.cpp
    src/kernel/libkernel.cpp
    src/kernel/time_hooks.cpp
    src/pad/pad_handler.cpp
    src/video/video_out.cpp
    src/video/vulkan_context.cpp
    src/video/font_service.cpp
    src/audio/audio_out.cpp
    src/gpu/gnm_driver.cpp
    src/kernel/save_data.cpp
    src/kernel/net_service.cpp
    src/kernel/np_service.cpp
    src/video/image_decoder.cpp
    src/kernel/dialog_service.cpp
    src/video/avplayer_service.cpp
    src/kernel/rtc_service.cpp
    src/kernel/user_service.cpp
    src/kernel/http_service.cpp
    src/kernel/security_service.cpp
)

set(HEADERS
    src/common/types.h
    src/common/logger.h
    src/common/crash_handler.h
    src/memory/vm.h
    src/loader/elf_loader.h
    src/loader/prx_module.h
    src/loader/nid_table.h
    src/kernel/libkernel.h
    src/kernel/time_hooks.h
    src/pad/pad_handler.h
    src/video/video_out.h
    src/video/vulkan_context.h
    src/video/font_service.h
    src/audio/audio_out.h
    src/gpu/gnm_driver.h
    src/kernel/save_data.h
    src/kernel/net_service.h
    src/kernel/np_service.h
    src/video/image_decoder.h
    src/kernel/dialog_service.h
    src/video/avplayer_service.h
    src/kernel/rtc_service.h
    src/kernel/user_service.h
    src/kernel/http_service.h
    src/kernel/security_service.h
)

add_executable(gr2_pc ${SOURCES} ${HEADERS})

target_link_libraries(gr2_pc PRIVATE
    xinput
    onecore
    user32
    gdi32
    rpcrt4
    ole32
    ws2_32
)

set_target_properties(gr2_pc PROPERTIES
    OUTPUT_NAME "gr2_pc"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
