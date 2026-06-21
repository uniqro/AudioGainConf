cmake_minimum_required(VERSION 3.20)
project(AudioDspServer C)

# ─────────────────────────────────────────────────────────────────────────────
# Source files
#
# AudioConf/ originals are NEVER modified.  All VxWorks compatibility is
# supplied by the shadow headers under pc_port/include/ (found first via
# target_include_directories below).
# ─────────────────────────────────────────────────────────────────────────────
set(AUDIO_SRCS
    AudioConf/AudioConf.c
    AudioConf/AudioConfigTask.c
    AudioConf/AudioConfigHandler.c
    AudioConf/AudioParamDef.c
    AudioConf/AudioSpiGateway.c
    AudioConf/AudioFlashMgr.c
)

set(PORT_SRCS
    pc_port/src/tasklib_stubs.c
    pc_port/src/flash_stubs.c
    pc_port/src/spi_stubs.c
    pc_port/src/dispatch_glue.c
    pc_port/main.c
)

add_executable(audio_dsp_server ${AUDIO_SRCS} ${PORT_SRCS})

# ─────────────────────────────────────────────────────────────────────────────
# Include search path
#
# pc_port/include MUST come before the project root so that stub headers
# shadow the VxWorks originals (e.g. <vxWorks.h> → our typedef shim).
# ─────────────────────────────────────────────────────────────────────────────
target_include_directories(audio_dsp_server PRIVATE
    ${CMAKE_SOURCE_DIR}/pc_port/include
    ${CMAKE_SOURCE_DIR}
)

# ─────────────────────────────────────────────────────────────────────────────
# Force-include vxWorks.h into every translation unit
#
# Injects VxWorks type aliases (STATUS, UINT8 …) and logMsg without requiring
# an explicit #include in the original source files.
#   GCC/Clang : -include <file>
#   MSVC       : /FI<file>
# ─────────────────────────────────────────────────────────────────────────────
set(VXWORKS_STUB "${CMAKE_SOURCE_DIR}/pc_port/include/vxWorks.h")

if(MSVC)
    target_compile_options(audio_dsp_server PRIVATE
        /FI${VXWORKS_STUB}
        /wd4005   # 'ERROR': macro redefined by wingdi.h; restored in sockLib.h
        /wd4047   # 'int' and pointer indirection levels differ
        /wd4028   # formal parameter type mismatch
        /wd4100   # unreferenced formal parameter
        /wd4311   # pointer truncation to 'int'
        /wd4024   # different types for formal/actual parameter
        /wd4133   # incompatible types — function pointer vs int
        /wd4090   # 'function' : different 'const' qualifiers
    )
else()
    # GCC / Clang — MinGW (Windows) or native (Linux)
    target_compile_options(audio_dsp_server PRIVATE
        -include "${VXWORKS_STUB}"
        -Wno-int-conversion
        -Wno-unused-parameter
        -Wno-pointer-compare
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Preprocessor definitions
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32)
    target_compile_definitions(audio_dsp_server PRIVATE
        _WIN32_WINNT=0x0601   # Windows 7+ Winsock2 API
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Link libraries
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(audio_dsp_server PRIVATE ws2_32)
else()
    target_link_libraries(audio_dsp_server PRIVATE pthread m)
endif()
