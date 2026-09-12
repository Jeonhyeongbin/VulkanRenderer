# ============================================================
# VulkanRenderer - Android 빌드
# android/ 폴더 안에 위치, 소스는 ../Project1/ 참조
# ============================================================
cmake_minimum_required(VERSION 3.15)
project(VulkanRenderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------
# Project1 소스 루트 경로
# -------------------------------------------------------
get_filename_component(PROJECT1_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../Project1" ABSOLUTE)

# -------------------------------------------------------
# native_app_glue 정적 라이브러리
# -------------------------------------------------------
set(ANDROID_NATIVE_APP_GLUE ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(native_app_glue STATIC
    ${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c
)

target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NATIVE_APP_GLUE}
)

# -------------------------------------------------------
# 소스 파일 (Project1/ 기준 경로)
# -------------------------------------------------------
set(MAIN_SOURCES
    ${PROJECT1_DIR}/AndroidAssets.cpp
    ${PROJECT1_DIR}/BaseRenderSystem.cpp
    ${PROJECT1_DIR}/BloomRenderSystem.cpp
    ${PROJECT1_DIR}/Buffer.cpp
    ${PROJECT1_DIR}/Camera.cpp
    ${PROJECT1_DIR}/ComputerShadeSystem.cpp
    ${PROJECT1_DIR}/GBufferPass.cpp
    ${PROJECT1_DIR}/DeferredLightingPass.cpp
    ${PROJECT1_DIR}/RenderGraph/RenderGraph.cpp
    ${PROJECT1_DIR}/RenderGraph/RenderGraphUtil.cpp
    ${PROJECT1_DIR}/DescriptorSet.cpp
    ${PROJECT1_DIR}/RenderGraph/VkSync.cpp
    ${PROJECT1_DIR}/Descriptors.cpp
    ${PROJECT1_DIR}/Device.cpp
    ${PROJECT1_DIR}/FrameInfo.cpp
    ${PROJECT1_DIR}/GameObject.cpp
    ${PROJECT1_DIR}/GameObjectManager.cpp
    ${PROJECT1_DIR}/ImGuiContext.cpp
    ${PROJECT1_DIR}/InputController.cpp
    ${PROJECT1_DIR}/JHBApplication.cpp
    ${PROJECT1_DIR}/Model.cpp
    ${PROJECT1_DIR}/PickingPass.cpp
    ${PROJECT1_DIR}/PBRPass.cpp
    ${PROJECT1_DIR}/BRDFLutPass.cpp
    ${PROJECT1_DIR}/IrradiancePass.cpp
    ${PROJECT1_DIR}/PrefilterPass.cpp
    ${PROJECT1_DIR}/Pipeline.cpp
    ${PROJECT1_DIR}/PipelineDesc.cpp
    ${PROJECT1_DIR}/ShaderReflection.cpp
    ${PROJECT1_DIR}/External/SPIRV-Reflect/spirv_reflect.c
    ${PROJECT1_DIR}/PointLightPass.cpp
    ${PROJECT1_DIR}/Renderer.cpp
    ${PROJECT1_DIR}/Scene.cpp
    ${PROJECT1_DIR}/ShadowPass.cpp
    ${PROJECT1_DIR}/SkyboxPass.cpp
    ${PROJECT1_DIR}/SwapChain.cpp
    ${PROJECT1_DIR}/Window.cpp
)

set(IMGUI_SOURCES
    ${PROJECT1_DIR}/External/Imgui/imgui.cpp
    ${PROJECT1_DIR}/External/Imgui/imgui_demo.cpp
    ${PROJECT1_DIR}/External/Imgui/imgui_draw.cpp
    ${PROJECT1_DIR}/External/Imgui/imgui_impl_vulkan.cpp
    ${PROJECT1_DIR}/External/Imgui/imgui_tables.cpp
    ${PROJECT1_DIR}/External/Imgui/imgui_widgets.cpp
)

# 진입점 (Main.cpp의 #ifdef ANDROID 분기 사용)
set(ANDROID_SOURCES
    ${PROJECT1_DIR}/Main.cpp
)

# -------------------------------------------------------
# Android는 공유 라이브러리(.so)로 빌드
# -------------------------------------------------------
add_library(VulkanRenderer SHARED
    ${MAIN_SOURCES}
    ${IMGUI_SOURCES}
    ${ANDROID_SOURCES}
)

# -------------------------------------------------------
# 인클루드 경로
# -------------------------------------------------------
target_include_directories(VulkanRenderer PRIVATE
    ${PROJECT1_DIR}
    ${PROJECT1_DIR}/External/Imgui
    ${PROJECT1_DIR}/External/glm
    ${PROJECT1_DIR}/External/tinyObjLoader
    ${PROJECT1_DIR}/External/tinygltf
    ${PROJECT1_DIR}/External/textureLoader
    ${PROJECT1_DIR}/External/KTX-Software/include
    ${ANDROID_NATIVE_APP_GLUE}
)

# -------------------------------------------------------
# KTX 공유 라이브러리 (jniLibs에서 ABI별로 로드)
# -------------------------------------------------------
add_library(ktx SHARED IMPORTED)
set_target_properties(ktx PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/jniLibs/${ANDROID_ABI}/libktx.so"
)

# -------------------------------------------------------
# 라이브러리 링크
# -------------------------------------------------------
target_link_libraries(VulkanRenderer PRIVATE
    -Wl,--whole-archive native_app_glue -Wl,--no-whole-archive
    android
    log
    vulkan
    ktx
)

# Android 15+ 16KB 페이지 크기 대응 + ANativeActivity_onCreate export 보장
target_link_options(VulkanRenderer PRIVATE
    "-Wl,-z,max-page-size=16384"
    "-Wl,-u,ANativeActivity_onCreate"
)

# -------------------------------------------------------
# 컴파일러 정의
# -------------------------------------------------------
target_compile_definitions(VulkanRenderer PRIVATE
    PLATFORM_ANDROID
    ANDROID
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

message(STATUS "=== VulkanRenderer Android 빌드 설정 완료 ===")
message(STATUS "  ABI        : ${ANDROID_ABI}")
message(STATUS "  API Level  : ${ANDROID_PLATFORM}")
message(STATUS "  Project1   : ${PROJECT1_DIR}")
