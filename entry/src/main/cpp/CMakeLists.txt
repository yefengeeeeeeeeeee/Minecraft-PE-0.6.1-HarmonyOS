cmake_minimum_required(VERSION 3.5.0)
project(mcpe)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(MCPE_SRC_ROOT ${NATIVERENDER_ROOT_PATH}/../../../../third_party/mcpe/src)

set(OHOS_SYSROOT_INCLUDE "C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/native/sysroot/usr/include")

include_directories(
    ${NATIVERENDER_ROOT_PATH}
    ${NATIVERENDER_ROOT_PATH}/GLES
    ${MCPE_SRC_ROOT}
    ${OHOS_SYSROOT_INCLUDE}
)

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

add_library(entry SHARED
    main_ohos.cpp
    AppPlatform_ohos.cpp
    SoundSystemOHOS.cpp

    ${MCPE_SOURCES}
)

target_compile_options(entry PRIVATE -DOHOS -Wno-narrowing)

target_link_libraries(entry PUBLIC
    libohinput.so
    libace_napi.z.so
    libhilog_ndk.z.so
    libEGL.so
    libGLESv3.so
    libnative_window.so
    librawfile.z.so
    libimage_source_ndk.z.so
    libpixelmap_ndk.z.so
    libz.so
    libohaudio.so
)
