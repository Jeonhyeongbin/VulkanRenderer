# ============================================================
# VulkanRenderer - Project1
# ============================================================
cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# -------------------------------------------------------
# Vulkan SDK 경로 확인
# -------------------------------------------------------
if(DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_PATH "$ENV{VULKAN_SDK}")
    message(STATUS "Vulkan SDK found: ${VULKAN_SDK_PATH}")
else()
    message(FATAL_ERROR
        "VULKAN_SDK 환경 변수가 설정되어 있지 않습니다.\n"
        "https://vulkan.lunarg.com/sdk/home 에서 Vulkan SDK를 설치하세요.")
endif()

# -------------------------------------------------------
# 소스 파일
# -------------------------------------------------------
set(MAIN_SOURCES
    BaseRenderSystem.cpp
    BloomRenderSystem.cpp
    Buffer.cpp
    Camera.cpp
    ComputerShadeSystem.cpp
    GBufferPass.cpp
    DeferredLightingPass.cpp
    RenderGraph/RenderGraph.cpp
    RenderGraph/RenderGraphUtil.cpp
    DescriptorSet.cpp
    RenderGraph/VkSync.cpp
    Descriptors.cpp
    Device.cpp
    FrameInfo.cpp
    GameObject.cpp
    GameObjectManager.cpp
    ImguiRenderSystem.cpp
    InputController.cpp
    JHBApplication.cpp
    Main.cpp
    Model.cpp
    MousePickingRenderSystem.cpp
    PBRPass.cpp
    PBRResourceGenerator.cpp
    Pipeline.cpp
    PipelineDesc.cpp
    ShaderReflection.cpp
    External/SPIRV-Reflect/spirv_reflect.c
    PointLightPass.cpp
    Renderer.cpp
    Scene.cpp
    ShadowPass.cpp
    SkyboxPass.cpp
    SwapChain.cpp
    Window.cpp
)

set(IMGUI_SOURCES
    External/Imgui/imgui.cpp
    External/Imgui/imgui_demo.cpp
    External/Imgui/imgui_draw.cpp
    External/Imgui/imgui_impl_glfw.cpp
    External/Imgui/imgui_impl_vulkan.cpp
    External/Imgui/imgui_tables.cpp
    External/Imgui/imgui_widgets.cpp
)

# -------------------------------------------------------
# 실행 파일 타겟
# -------------------------------------------------------
add_executable(VulkanRenderer
    ${MAIN_SOURCES}
    ${IMGUI_SOURCES}
)

# -------------------------------------------------------
# 인클루드 경로
# -------------------------------------------------------
target_include_directories(VulkanRenderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}                          # 프로젝트 루트 헤더
    ${CMAKE_CURRENT_SOURCE_DIR}/External/Imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/External/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/External/glfw-3.4.bin.WIN64/include
    ${CMAKE_CURRENT_SOURCE_DIR}/External/tinyObjLoader
    ${CMAKE_CURRENT_SOURCE_DIR}/External/tinygltf
    ${CMAKE_CURRENT_SOURCE_DIR}/External/textureLoader
    ${CMAKE_CURRENT_SOURCE_DIR}/External/KTX-Software/include
    "${VULKAN_SDK_PATH}/Include"
)

# -------------------------------------------------------
# 라이브러리 디렉터리
# -------------------------------------------------------
target_link_directories(VulkanRenderer PRIVATE
    "${VULKAN_SDK_PATH}/Lib"
    ${CMAKE_CURRENT_SOURCE_DIR}/External/glfw-3.4.bin.WIN64/lib-vc2022
    ${CMAKE_CURRENT_SOURCE_DIR}/External/KTX-Software/lib
)

# -------------------------------------------------------
# 링크 라이브러리
# -------------------------------------------------------
add_custom_command(TARGET VulkanRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/External/KTX-Software/lib/ktx.dll"
        "$<TARGET_FILE_DIR:VulkanRenderer>"
    COMMENT "ktx.dll 복사 중..."
)

target_link_libraries(VulkanRenderer PRIVATE
    vulkan-1
    glfw3
    ktx
)

# -------------------------------------------------------
# 컴파일러 설정
# -------------------------------------------------------
target_compile_definitions(VulkanRenderer PRIVATE
    WIN32
    _CONSOLE
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

if(MSVC)
    set_source_files_properties(Device.cpp Renderer.cpp PROPERTIES COMPILE_OPTIONS "/utf-8")
    target_compile_options(VulkanRenderer PRIVATE
        /W3         # 경고 수준 3
        /sdl        # SDL 보안 검사
        /MP         # 멀티 프로세서 컴파일
        $<$<CONFIG:Release>:/O2 /GL>   # Release 최적화
    )
    target_link_options(VulkanRenderer PRIVATE
        $<$<CONFIG:Release>:/LTCG>     # Release 링크 타임 최적화
    )
endif()

# -------------------------------------------------------
# 셰이더 컴파일 (glslc 사용)
# -------------------------------------------------------
find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS "${VULKAN_SDK_PATH}/Bin" "${VULKAN_SDK_PATH}/bin"
)

if(GLSLC_EXECUTABLE)
    message(STATUS "glslc found: ${GLSLC_EXECUTABLE}")

    file(GLOB_RECURSE SHADER_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
    )

    set(SHADER_SPV_FILES "")
    foreach(SHADER_FILE ${SHADER_FILES})
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
        set(SPV_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT  ${SPV_OUTPUT}
            COMMAND ${GLSLC_EXECUTABLE} "${SHADER_FILE}" -o "${SPV_OUTPUT}"
            DEPENDS ${SHADER_FILE}
            COMMENT "셰이더 컴파일: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
            VERBATIM
        )
        list(APPEND SHADER_SPV_FILES ${SPV_OUTPUT})
    endforeach()

    add_custom_target(CompileShaders ALL
        DEPENDS ${SHADER_SPV_FILES}
        COMMENT "모든 GLSL 셰이더 컴파일"
    )

    add_dependencies(VulkanRenderer CompileShaders)
else()
    message(WARNING
        "glslc를 찾을 수 없습니다. 셰이더가 자동으로 컴파일되지 않습니다.\n"
        "Vulkan SDK가 올바르게 설치되어 있는지 확인하세요.")
endif()

# -------------------------------------------------------
# 빌드 출력 경로
# -------------------------------------------------------
set_target_properties(VulkanRenderer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

message(STATUS "=== VulkanRenderer 빌드 설정 완료 ===")
message(STATUS "  빌드 타입  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Vulkan SDK : ${VULKAN_SDK_PATH}")
message(STATUS "  출력 경로  : ${CMAKE_BINARY_DIR}/bin")
