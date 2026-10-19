cmake_minimum_required(VERSION 3.5.0)
project(mcpe)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(MCPE_SRC_ROOT ${NATIVERENDER_ROOT_PATH}/../../../../third_party/mcpe/src)

set(OHOS_SYSROOT_INCLUDE "C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/include")

include_directories(
    ${NATIVERENDER_ROOT_PATH}
    ${NATIVERENDER_ROOT_PATH}/GLES
    ${MCPE_SRC_ROOT}
)

# 该路径是 Windows 上 DevEco 的 NDK sysroot 头目录，换平台后不存在。
# 存在才 include（NDK 工具链本身已经带了 sysroot，这里只是兜底）。
if(EXISTS "${OHOS_SYSROOT_INCLUDE}")
    include_directories(${OHOS_SYSROOT_INCLUDE})
endif()

file(GLOB_RECURSE MCPE_SOURCES CONFIGURE_DEPENDS
    ${MCPE_SRC_ROOT}/*.cpp
)

# Exclude platform-specific entry points / backends not used on OHOS
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/main\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/main_android\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/main_android_java\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/main_rpi\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/main_dedicated\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/main_glfw\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/AppPlatform_win32\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/AppPlatform_glfw\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/SoundSystemSL\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/SoundSystemAL\\.cpp$")
list(FILTER MCPE_SOURCES EXCLUDE REGEX "/PngLoader\\.cpp$")

add_library(minecraftpe SHARED
    main_ohos.cpp
    AppPlatform_ohos.cpp
    SoundSystemOHOS.cpp

    ${MCPE_SOURCES}
)

target_compile_options(minecraftpe PRIVATE -DOHOS -Wno-narrowing)

target_link_libraries(minecraftpe PUBLIC
    libohinput.so
    libace_napi.z.so
    libhilog_ndk.z.so
    libnet_connection.so
    libEGL.so
    libGLESv3.so
    libnative_window.so
    librawfile.z.so
    libimage_source_ndk.z.so
    libpixelmap_ndk.z.so
    libz.so
    libohaudio.so
)
