# arm-none-eabi-toolchain.cmake
# Minimal ARM cross-compilation toolchain for PY32F071 (UV-K5 V3 / UV-K1).
# Used by GitHub Actions CI to override the Fusion preset's Docker-specific
# toolchain path, which doesn't exist on the Actions runner.

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# arm-none-eabi-gcc is placed on PATH by carlosperate/arm-none-eabi-gcc-action
set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)

# Without this, CMake tries to link a test executable against libc/startup
# which doesn't exist for a bare-metal target — this skips that check.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Ensure CMake knows this is a cross-compilation (makes add_executable visible)
set(CMAKE_CROSSCOMPILING TRUE)
