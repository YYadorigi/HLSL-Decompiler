cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0175 OLD)
project(HLSLDecompiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Force Release build for main project" FORCE)

# Use ExternalProject to avoid third-party conflicts
include(ExternalProject)

# Step 1: Update submodules
find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE result
)
if(NOT result EQUAL "0")
    message(FATAL_ERROR "Failed to update submodules")
endif()

# Step 2: Use ExternalProject to build submodules
# dxil-spirv subproject
ExternalProject_Add(dxil_spirv_external
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendor/dxil-spirv
    BINARY_DIR ${CMAKE_BINARY_DIR}/vendor/dxil-spirv
    CONFIGURE_COMMAND ${CMAKE_COMMAND} 
        -B ${CMAKE_BINARY_DIR}/vendor/dxil-spirv 
        -S ${CMAKE_SOURCE_DIR}/vendor/dxil-spirv 
        -DCMAKE_BUILD_TYPE=Release
    BUILD_COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/vendor/dxil-spirv --target dxil-spirv dxil-spirv-c-shared --config Release
    INSTALL_COMMAND ""
)

# SPIRV-Cross subproject
ExternalProject_Add(spirv_cross_external
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendor/SPIRV-Cross
    BINARY_DIR ${CMAKE_BINARY_DIR}/vendor/SPIRV-Cross
    CONFIGURE_COMMAND ${CMAKE_COMMAND} 
        -B ${CMAKE_BINARY_DIR}/vendor/SPIRV-Cross 
        -S ${CMAKE_SOURCE_DIR}/vendor/SPIRV-Cross 
        -DCMAKE_BUILD_TYPE=Release
    BUILD_COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/vendor/SPIRV-Cross --target spirv-cross --config Release
    INSTALL_COMMAND ""
)

# DirectXShaderCompiler subproject
ExternalProject_Add(dxcompiler_external
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendor/DirectXShaderCompiler
    BINARY_DIR ${CMAKE_BINARY_DIR}/vendor/DirectXShaderCompiler
    CONFIGURE_COMMAND ${CMAKE_COMMAND} 
        -B ${CMAKE_BINARY_DIR}/vendor/DirectXShaderCompiler 
        -S ${CMAKE_SOURCE_DIR}/vendor/DirectXShaderCompiler 
        -DCMAKE_BUILD_TYPE=Release
        -DLLVM_TARGETS_TO_BUILD=None
        -DLLVM_INCLUDE_TESTS=OFF
        -DHLSL_INCLUDE_TESTS=OFF
        -DSPIRV_BUILD_TESTS=OFF
        -C ${CMAKE_SOURCE_DIR}/vendor/DirectXShaderCompiler/cmake/caches/PredefinedParams.cmake
    BUILD_COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/vendor/DirectXShaderCompiler --target dxbc2dxil dxcompiler dxilconv --config Release
    INSTALL_COMMAND ""
)

# Step 3: Add main project
add_executable(HLSLDecompiler main.cpp)

# Step 4: Set submodule paths
set(DXIL_SPIRV_EXE_PATH ${CMAKE_BINARY_DIR}/vendor/dxil-spirv/Release/dxil-spirv.exe)
set(DXIL_SPIRV_C_SHARED_DLL_PATH ${CMAKE_BINARY_DIR}/vendor/dxil-spirv/Release/dxil-spirv-c-shared.dll)
set(SPIRV_CROSS_EXE_PATH ${CMAKE_BINARY_DIR}/vendor/SPIRV-Cross/Release/spirv-cross.exe)
set(DXBC2DXIL_EXE_PATH ${CMAKE_BINARY_DIR}/vendor/DirectXShaderCompiler/Release/bin/dxbc2dxil.exe)
set(DXCOMPILER_DLL_PATH ${CMAKE_BINARY_DIR}/vendor/DirectXShaderCompiler/Release/bin/dxcompiler.dll)
set(DXILCONV_DLL_PATH ${CMAKE_BINARY_DIR}/vendor/DirectXShaderCompiler/Release/bin/dxilconv.dll)

# Step 5: Copy executable files to main output directory
add_custom_command(TARGET HLSLDecompiler POST_BUILD
    COMMENT "Copying submodule executables to main output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DXIL_SPIRV_EXE_PATH}
        $<TARGET_FILE_DIR:HLSLDecompiler>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DXIL_SPIRV_C_SHARED_DLL_PATH}
        $<TARGET_FILE_DIR:HLSLDecompiler>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPIRV_CROSS_EXE_PATH}
        $<TARGET_FILE_DIR:HLSLDecompiler>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DXBC2DXIL_EXE_PATH}
        $<TARGET_FILE_DIR:HLSLDecompiler>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DXCOMPILER_DLL_PATH}
        $<TARGET_FILE_DIR:HLSLDecompiler>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DXILCONV_DLL_PATH}
        $<TARGET_FILE_DIR:HLSLDecompiler>
    DEPENDS dxil_spirv_external spirv_cross_external dxcompiler_external
)

# Step 6: Generate batch script for RenderDoc plugin
set(BATCH_SCRIPT_CONTENT
"@echo off
setlocal enabledelayedexpansion

: Decompile DXBC/DXIL/SPIR-V to HLSL
\"%~dp0HLSLDecompiler.exe\" %* 

: Redirect to stdout
for %%f in (\"%1\") do type \"%%~dpnf.hlsl\"

endlocal"
)

file(WRITE ${CMAKE_BINARY_DIR}/HLSLDecompiler.bat "${BATCH_SCRIPT_CONTENT}")

add_custom_command(TARGET HLSLDecompiler POST_BUILD
    COMMENT "Generating batch script for RenderDoc plugin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/HLSLDecompiler.bat
        $<TARGET_FILE_DIR:HLSLDecompiler>
    COMMAND ${CMAKE_COMMAND} -E remove
        ${CMAKE_BINARY_DIR}/HLSLDecompiler.bat
    DEPENDS HLSLDecompiler
)
