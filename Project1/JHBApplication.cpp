#include "JHBApplication.h"
#include "GBufferPass.h"
#include "DeferredLightingPass.h"
#include "ShadowPass.h"
#include "SkyboxPass.h"
#include "PointLightPass.h"
#include "PresentPass.h"
#include "BRDFLutPass.h"
#include "IrradiancePass.h"
#include "PrefilterPass.h"
#ifndef ANDROID
#include "ImGuiPass.h"
#include "ImGuiContext.h"
#endif
#include "InputController.h"
#include "FrameInfo.h"
#include "Model.h"
#ifndef ANDROID
#include "External/Imgui/imgui.h"
#endif
#include "GameObjectManager.h"
#include "Scene.h"

#include <memory>
#include <unordered_set>
#include <cstring>
#include <limits>
#ifdef ANDROID
#include <thread>
#include <chrono>
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer", __VA_ARGS__)
#endif

namespace jhb
{
JHBApplication::JHBApplication()
{
}

JHBApplication::~JHBApplication()
{
    // Also covers exceptions leaving Run() before its normal idle wait.
    vkDeviceWaitIdle(device.getLogicalDevice());
    releaseDescriptors();
#ifndef ANDROID
    imguiContext.reset();
#endif
}

VkResult JHBApplication::Run()
{
    auto result = renderer.init();
    if (result != VK_SUCCESS)
    {
        return result;
    }
    uboBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    viewDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    GlobalScene = std::make_unique<Scene>(device);
    init();

    auto viewerObject = GameObject::createGameObject();
    viewerObject.transform.translation = {0.0f, 0.0f, -4.0f};
    viewerObject.transform.rotation = {0.0f, glm::radians(90.0f), 0.0f};
#ifndef ANDROID
    InputController cameraController{device.getWindow().GetGLFWwindow(), viewerObject};
#else
    InputController cameraController{viewerObject};
    inputController.store(&cameraController); // Main.cpp handleInputEvent에서 사용 (atomic)
#endif
    double x = 0, y = 0;
    auto currentTime = std::chrono::high_resolution_clock::now();

#ifndef ANDROID
    auto forwardDir = cameraController.move(&window.GetGLFWwindow(), 0, viewerObject);
#else
    auto forwardDir = cameraController.move(0, viewerObject);
#endif
    window.getCamera()->setViewDirection(viewerObject.transform.translation, forwardDir);
    float aspect = renderer.getAspectRatio();
    window.getCamera()->setPerspectiveProjection(aspect, 0.1f, 200.f);

#ifdef ANDROID
    // 30fps 캡용 타이머 초기화
    constexpr long long TARGET_FRAME_US = 33333LL; // 1/30초 = 33.33ms
    auto lastFrameEnd = std::chrono::high_resolution_clock::now();

    while (androidAppRunning.load())
    {
        // 이벤트 처리는 android_main 이벤트 스레드가 전담한다.
        // 이 스레드(앱 스레드)에서 ALooper_pollAll을 호출하면 안 된다.
        // (ALooper는 android_main 스레드가 소유)
        if (androidApp->destroyRequested)
        {
            break;
        }

        // ── 30fps 프레임 캡 (발열 감소) ──────────────────────────────────
        // 이전 프레임 끝에서 33.3ms가 지나지 않았으면 sleep으로 CPU를 쉬게 한다.
        // FIFO VSync로 GPU는 이미 60fps 제한이 걸리지만 CPU는 계속 스핀하므로
        // 명시적 sleep을 줘야 발열이 줄어든다.
        {
            auto now = std::chrono::high_resolution_clock::now();
            long long elapsedUs =
                std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameEnd).count();
            if (elapsedUs < TARGET_FRAME_US)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(TARGET_FRAME_US - elapsedUs));
            }
            lastFrameEnd = std::chrono::high_resolution_clock::now();
        }
        // ────────────────────────────────────────────────────────────────

        // 터치 입력에서 커서 위치 업데이트
        x = cameraController.getTouchX();
        y = cameraController.getTouchY();
#else
    while (!glfwWindowShouldClose(&window.GetGLFWwindow()))
    {
        glfwPollEvents(); // may block
        if (glfwWindowShouldClose(&window.GetGLFWwindow()))
        {
            break;
        }
        const auto windowExtent = window.getExtent();
        if (windowExtent.width == 0 || windowExtent.height == 0)
        {
            glfwWaitEventsTimeout(0.1);
            continue;
        }
        glfwGetCursorPos(&window.GetGLFWwindow(), &x, &y);
#endif
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime =
            std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime)
                .count();
        currentTime = newTime;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        result = renderer.beginFrame(commandBuffer);
        if (result == VK_NOT_READY)
        {
            result = VK_SUCCESS;
            continue;
        }
        if (result != VK_SUCCESS)
        {
            break;
        }

        int frameIndex = renderer.getFrameIndex();
        consumePicking(frameIndex);
        if (window.GetMousePressed())
        {
            window.mouseMove(x, y, frameTime, viewerObject);
        }

#ifndef ANDROID
        auto forwardDir = cameraController.move(&window.GetGLFWwindow(), frameTime, viewerObject);
#else
        auto forwardDir = cameraController.move(frameTime, viewerObject);
#endif
        window.getCamera()->setViewDirection(viewerObject.transform.translation, forwardDir);
        float aspect = renderer.getAspectRatio();
        window.getCamera()->setPerspectiveProjection(aspect, 0.1f, 200.f);
        updateViewBuffer(frameIndex);
        recordFrame(commandBuffer);
        result = renderer.endFrame();
        if (result == VK_NOT_READY)
        {
            result = VK_SUCCESS;
            continue;
        }
        if (result != VK_SUCCESS)
        {
            break;
        }
    }

    // Run() 루프 종료 후 dangling pointer 방지
    // (cameraController는 이 함수가 반환되면 소멸됨)
#ifdef ANDROID
    inputController.store(nullptr);
#endif
    const auto idle = vkDeviceWaitIdle(device.getLogicalDevice());
    return result != VK_SUCCESS ? result : idle;
}

void JHBApplication::init()
{
    for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
    {
        uboBuffers[frame] = std::make_unique<Buffer>(device, sizeof(GlobalUbo), 1,
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uboBuffers[frame]->map();
        auto &readback = pickingReadbacks[frame];
        readback.buffer =
            std::make_unique<Buffer>(device, sizeof(uint32_t), 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (readback.buffer->map() != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map picking readback");
        }
    }

    GlobalScene->loadDefaultScene();
    collectSceneModels();
    prepareIBL();
    if (GlobalScene->pointLights().empty())
    {
        GlobalScene->pointLights().push_back(
            {{-3.0f, 3.0f, -3.0f}, {1.0f, 1.0f, 1.0f}, 40.0f, 0.1f});
    }
#ifndef ANDROID
    imguiPass = std::make_unique<ImGuiPass>();
    // UI composites onto the deferred color target.
    imguiContext = std::make_unique<ImGuiContext>(
        device, VK_FORMAT_R16G16B16A16_SFLOAT,
        static_cast<uint32_t>(renderer.GetSwapChain().getImageCount()));
#endif
}

void JHBApplication::prepareIBL()
{
    iblGraph = std::make_unique<rg::RenderGraph>(device, 1);
    auto &graph = *iblGraph;
    iblTextures.environment =
        graph.importTexture("ibl/environment", GlobalScene->environmentTexture(),
                            vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);

    const auto create = [&](const char *name, uint32_t size, VkFormat format,
                            VkImageUsageFlags usage, uint32_t mips = 1, bool cube = false)
    {
        rg::ImageDesc desc;
        desc.width = desc.height = size;
        desc.format = format;
        desc.usage = usage;
        desc.mipLevels = mips;
        desc.arrayLayers = cube ? 6 : 1;
        desc.imageType = cube ? rg::ImageType::CubeMap : rg::ImageType::Image2D;
        return graph.createTexture(name, desc);
    };
    iblTextures.brdfLut = create("ibl/brdf-lut", 512, VK_FORMAT_R16G16_SFLOAT,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto cubeUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const auto scratchUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    iblTextures.irradiance =
        create("ibl/irradiance", 64, VK_FORMAT_R32G32B32A32_SFLOAT, cubeUsage, 7, true);
    const auto irradianceScratch =
        create("ibl/irradiance-scratch", 64, VK_FORMAT_R32G32B32A32_SFLOAT, scratchUsage);
    iblTextures.prefilter =
        create("ibl/prefilter", 512, VK_FORMAT_R16G16B16A16_SFLOAT, cubeUsage, 10, true);
    const auto prefilterScratch =
        create("ibl/prefilter-scratch", 512, VK_FORMAT_R16G16B16A16_SFLOAT, scratchUsage);

    const auto cube = GlobalScene->environmentCube();
    const auto drawCube =
        [cube](VkCommandBuffer cmd, const rg::RenderPass &pass, const rg::GraphResources &resources)
    {
        // Scene's cube has no material pipeline; filter passes own the face MVP push constants.
        cube->bind(cmd);
        cube->draw(cmd, resources.pipeline(pass.pipelineHandle).layout, 0);
    };
    BRDFLutPass{}.addPass(graph, iblTextures.brdfLut);
    IrradiancePass{}.addPass(
        graph, {iblTextures.environment, irradianceScratch, iblTextures.irradiance}, drawCube);
    PrefilterPass{}.addPass(
        graph, {iblTextures.environment, prefilterScratch, iblTextures.prefilter}, drawCube);
    graph.prepare();
}

void JHBApplication::recordIBL(VkCommandBuffer commandBuffer)
{
    if (iblRecorded)
    {
        return;
    }
    rg::RenderContext context;
    // No presentation pass: all initialization attachments belong to the IBL graph.
    iblGraph->render(renderer, commandBuffer, context, {});
    for (const auto id : {iblTextures.brdfLut, iblTextures.irradiance, iblTextures.prefilter})
    {
        auto &output = iblGraph->resources().texture(id);
        const auto &image = output.texture.image;
        output.previousAccess = vk_sync::imagePipelineBarrier(
            commandBuffer, image.image,
            {image.desc.aspectFlags, 0, image.desc.mipLevels, 0, image.desc.arrayLayers},
            output.previousAccess,
            vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer, false);
    }
    // Recorded, not CPU-fence-complete. The frame graph follows in the same command buffer.
    // Retain the initialization graph/pools/scratch until shutdown after GPU idle.
    iblRecorded = true;
}

void JHBApplication::collectSceneModels()
{
    // One draw per shared instanced model, not one draw per GameObject.
    sceneModels.clear();
    std::unordered_set<Model *> seen;
    for (const auto &[id, object] : GameObjectManager::GetSingleton().gameObjects)
    {
        if (object.model && !object.model->nodes.empty() && seen.insert(object.model.get()).second)
        {
            if (!object.model->instanceBuffer)
            {
                throw std::logic_error("Prepare scene instance buffers before graph rendering");
            }
            sceneModels.push_back(object.model);
        }
    }
}

void JHBApplication::prepareDescriptors(const rg::Pipeline &pipeline)
{
    if (descriptorsReady)
    {
        return;
    }
    for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
    {
        auto &set = viewDescriptorSets[frame];
        set =
            pipeline.allocateDescriptorSet(device.getLogicalDevice(), rg::kDescriptorSetIndexView);
        set.writeBuffer(0, uboBuffers[frame]->descriptorInfo());
    }
    for (const auto &model : sceneModels)
    {
        for (auto &material : model->materials)
        {
            auto images = GlobalScene->materialImages(*model, material);
            for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
            {
                auto set = pipeline.allocateDescriptorSet(device.getLogicalDevice(), 3);
                for (uint32_t binding = 0; binding < images.size(); ++binding)
                {
                    set.writeImage(binding, images[binding]);
                }
                // Model keeps a non-owning handle for its existing draw interface.
                material.descriptorSets.at(frame) = set.handle;
                materialDescriptorSets.push_back(std::move(set));
            }
        }
    }
    descriptorsReady = true;
}

void JHBApplication::releaseDescriptors()
{
    for (const auto &model : sceneModels)
    {
        for (auto &material : model->materials)
        {
            std::fill(material.descriptorSets.begin(), material.descriptorSets.end(),
                      VK_NULL_HANDLE);
        }
    }
    materialDescriptorSets.clear();
    for (auto &set : viewDescriptorSets)
    {
        set.reset();
    }
    descriptorsReady = false;
}

void JHBApplication::updateViewBuffer(int frameIndex)
{
    GlobalUbo ubo{};
    ubo.projection = window.getCamera()->getProjection();
    ubo.view = window.getCamera()->getView();
    ubo.inverseView = window.getCamera()->getInverseView();
    for (const auto &light : GlobalScene->pointLights())
    {
        if (ubo.numLights == MaxLights)
        {
            break;
        }
        auto &target = ubo.pointLights[ubo.numLights++];
        target.position = glm::vec4(light.position, 1.0f);
        target.color = glm::vec4(light.color, light.intensity);
    }
    uboBuffers[frameIndex]->writeToBuffer(&ubo);
    uboBuffers[frameIndex]->flush();
}

void JHBApplication::ensureFrameGraph()
{
    const auto generation = renderer.getSwapChainGeneration();
    if (frameGraph && graphGeneration == generation)
    {
        return;
    }
    // Conservative resize policy: discard old graph caches only once GPU use has ended.
    if (vkDeviceWaitIdle(device.getLogicalDevice()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to wait before rebuilding frame graph");
    }
    // Drop sets before their pipeline layouts, after all GPU use has completed.
    releaseDescriptors();
    presentPass.reset();
    for (auto &readback : pickingReadbacks)
    {
        readback.pending = false;
        readback.targets.clear();
    }
    for (auto &pass : lightingPasses)
    {
        pass.reset();
    }
    for (auto &pass : gbufferPasses)
    {
        pass.reset();
    }
    frameGraph = std::make_unique<rg::RenderGraph>(device, SwapChain::MAX_FRAMES_IN_FLIGHT);
    const auto importIBL = [&](const char *name, rg::TextureId id)
    {
        const auto &source = iblGraph->resources().texture(id);
        return frameGraph->importTexture(name, source.texture, source.previousAccess);
    };
    frameIBLTextures.environment = importIBL("ibl/environment", iblTextures.environment);
    frameIBLTextures.brdfLut = importIBL("ibl/brdf-lut", iblTextures.brdfLut);
    frameIBLTextures.irradiance = importIBL("ibl/irradiance", iblTextures.irradiance);
    frameIBLTextures.prefilter = importIBL("ibl/prefilter", iblTextures.prefilter);
    rg::PipelineDesc geometry = rg::PipelineDesc::builder().defaultMeshVertexInput();
    geometry.reflectShaders = false; // GBufferPass supplies its explicit descriptor contract.
    const auto depthFormat = device.findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}, VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
    {
        gbufferPasses[frame] =
            std::make_unique<GBufferPass>(*frameGraph, renderer.getSwapChainExtent(), geometry,
                                          depthFormat, "gbuffer/frame-" + std::to_string(frame));
        lightingPasses[frame] = std::make_unique<DeferredLightingPass>(
            *frameGraph, renderer.getSwapChainExtent(), "lighting/frame-" + std::to_string(frame));
        rg::ImageDesc shadow;
        shadow.width = shadow.height = 512;
        shadow.format = VK_FORMAT_R32_SFLOAT;
        shadow.arrayLayers = 6;
        shadow.imageType = rg::ImageType::CubeMap;
        shadow.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        shadowCubes[frame] = frameGraph->createTexture("shadow/" + std::to_string(frame), shadow);
        shadow.format = depthFormat;
        shadow.arrayLayers = 1;
        shadow.imageType = rg::ImageType::Image2D;
        shadow.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        shadow.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        shadowDepths[frame] =
            frameGraph->createTexture("shadow-depth/" + std::to_string(frame), shadow);
        rg::ImageDesc picking;
        const auto extent = renderer.getSwapChainExtent();
        picking.width = extent.width;
        picking.height = extent.height;
        picking.format = VK_FORMAT_R32_UINT;
        picking.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        pickingImages[frame].ids =
            frameGraph->createTexture("picking/ids/" + std::to_string(frame), picking);
        picking.format = depthFormat;
        picking.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        picking.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        pickingImages[frame].depth =
            frameGraph->createTexture("picking/depth/" + std::to_string(frame), picking);
        auto &readback = pickingReadbacks[frame];
        rg::Buffer buffer;
        buffer.buffer = readback.buffer->getBuffer();
        buffer.size = readback.buffer->getBufferSize();
        buffer.usage = readback.buffer->getUsageFlags();
        readback.graphBuffer = frameGraph->importBuffer("picking/readback/" + std::to_string(frame),
                                                        buffer, vk_sync::AccessType::HostRead);
    }
    presentPass = std::make_unique<PresentPass>(*frameGraph,
                                                renderer.GetSwapChain().getSwapChainImageFormat());
    graphGeneration = generation;
}

void JHBApplication::recordFrame(VkCommandBuffer commandBuffer)
{
    recordIBL(commandBuffer);
    const bool resized = frameGraph && graphGeneration != renderer.getSwapChainGeneration();
    ensureFrameGraph();
    const int frameIndex = renderer.getFrameIndex();
    auto &graph = *frameGraph;
    graph.newFrame(frameIndex);
    graph.clear();
    ShadowPass{}.addPass(
        graph, {shadowCubes[frameIndex], shadowDepths[frameIndex]},
        {GlobalScene->pointLights().front().position, 0.1f, 200.0f},
        [this](VkCommandBuffer cmd, const rg::RenderPass &pass, const rg::GraphResources &resources)
        {
            for (const auto &model : sceneModels)
            {
                model->drawGraphGeometryOnly(cmd, resources.pipeline(pass.pipelineHandle).layout);
            }
        });
    gbufferPasses[frameIndex]->addPass(
        [this, frameIndex](VkCommandBuffer cmd, const rg::RenderPass &pass,
                           const rg::GraphResources &resources)
        {
            const auto layout = resources.pipeline(pass.pipelineHandle).layout;
            for (const auto &model : sceneModels)
            {
                model->drawGraphGeometry(cmd, layout, frameIndex, true);
            }
        });
    const auto geometryPipeline = graph.passes().back().pipelineHandle;
    lightingPasses[frameIndex]->addPass(gbufferPasses[frameIndex]->output(),
                                        {frameIBLTextures.brdfLut, frameIBLTextures.irradiance,
                                         frameIBLTextures.prefilter, shadowCubes[frameIndex]});
    const auto previewColor = lightingPasses[frameIndex]->output();
    const auto depth = gbufferPasses[frameIndex]->output().depth;
    SkyboxPass{}.addPass(
        graph, {frameIBLTextures.environment, previewColor, depth},
        [cube = GlobalScene->environmentCube()](VkCommandBuffer cmd, const rg::RenderPass &pass,
                                                const rg::GraphResources &resources)
        {
            cube->bind(cmd);
            cube->draw(cmd, resources.pipeline(pass.pipelineHandle).layout, 0);
        });
    PointLightPass{}.addPass(graph, {previewColor, depth}, GlobalScene->pointLights());
    std::optional<VkOffset2D> pickPixel;
#ifndef ANDROID
    imguiContext->configureTarget(graph.resources().texture(previewColor).texture.image.desc.format,
                                  static_cast<uint32_t>(renderer.GetSwapChain().getImageCount()));
    imguiContext->beginFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 300.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("settings", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::TextUnformatted("Deferred lighting + IBL + shadows");
    ImGui::Text("Models: %d", static_cast<int>(sceneModels.size()));
    if (const auto selected = GlobalScene->selectedObject())
    {
        ImGui::Text("Selected object: %u", *selected);
    }
    else
    {
        ImGui::TextUnformatted("Selected object: none");
    }
    ImGui::SliderFloat("roughness", &uiRoughness, 0.1f, 1.0f);
    ImGui::SliderFloat("metallic", &uiMetallic, 0.1f, 1.0f);
    ImGui::End();
    if (!resized && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::GetIO().WantCaptureMouse)
    {
        int width = 0, height = 0;
        glfwGetWindowSize(&window.GetGLFWwindow(), &width, &height);
        const auto mouse = ImGui::GetIO().MousePos;
        pickPixel =
            PickingPass::pixel(mouse.x, mouse.y, width, height, renderer.getSwapChainExtent());
    }
    imguiContext->endFrame();
    imguiPass->addPass(graph, previewColor, imguiContext->drawFunction());
#endif
    if (pickPixel)
    {
        auto &readback = pickingReadbacks[frameIndex];
        readback.targets.clear();
        std::vector<std::pair<std::shared_ptr<Model>, uint32_t>> draws;
        for (const auto &model : sceneModels)
        {
            auto targets = GlobalScene->pickingTargets(model);
            if (readback.targets.size() + targets.size() >= (std::numeric_limits<uint32_t>::max)())
            {
                throw std::overflow_error("Too many picking instances");
            }
            draws.emplace_back(model, static_cast<uint32_t>(readback.targets.size() + 1));
            readback.targets.insert(readback.targets.end(), targets.begin(), targets.end());
        }
        PickingPass{}.addPass(
            graph, pickingImages[frameIndex],
            [draws = std::move(draws)](VkCommandBuffer cmd, const rg::RenderPass &pass,
                                       const rg::GraphResources &resources)
            {
                const auto layout = resources.pipeline(pass.pipelineHandle).layout;
                for (const auto &[model, baseId] : draws)
                {
                    PickingPass::pushBaseId(cmd, layout, baseId);
                    model->drawGraphGeometryOnly(cmd, layout);
                }
            },
            PickingPass::Readback{readback.graphBuffer, *pickPixel});
        readback.request = ++latestPickRequest;
        readback.generation = graphGeneration;
    }
    presentPass->addPass(previewColor);
    graph.prepare();
    prepareDescriptors(graph.resources().pipeline(geometryPipeline));

    // The acquired image index is independent of the frame-in-flight slot.
    auto &swapChain = renderer.GetSwapChain();
    const auto imageIndex = static_cast<uint32_t>(renderer.getImageIndex());
    const auto extent = renderer.getSwapChainExtent();
    rg::Image presentImage;
    presentImage.image = swapChain.getImage(imageIndex);
    presentImage.imageView = swapChain.getSwapChianImageView(imageIndex);
    presentImage.desc.width = extent.width;
    presentImage.desc.height = extent.height;
    presentImage.desc.format = swapChain.getSwapChainImageFormat();
    presentImage.desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    rg::RenderContext context;
    for (const auto &set : viewDescriptorSets)
    {
        context.viewDescriptorSets.push_back(set.handle);
    }
    graph.render(renderer, commandBuffer, context, presentImage);
    if (pickPixel)
    {
        auto &readback = pickingReadbacks[frameIndex];
        // endFrame submits this copy with the existing frame fence. Failure exits Run.
        readback.pending = true;
    }
    vk_sync::imagePipelineBarrier(
        commandBuffer, presentImage.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        vk_sync::AccessType::ColorAttachmentWrite, vk_sync::AccessType::Present, false);
}

void JHBApplication::consumePicking(int frameIndex)
{
    auto &readback = pickingReadbacks[frameIndex];
    if (!readback.pending)
    {
        return;
    }
    // beginFrame has successfully waited for this slot's previous graphics submission.
    readback.pending = false;
    if (readback.generation == renderer.getSwapChainGeneration() &&
        readback.request == latestPickRequest)
    {
        // Buffer maps its entire dedicated allocation; whole-range invalidation is aligned.
        if (readback.buffer->invalidate() != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to invalidate picking readback");
        }
        uint32_t id = 0;
        std::memcpy(&id, readback.buffer->getMappedMemory(), sizeof(id));
        GlobalScene->select(id > 0 && id <= readback.targets.size()
                                ? std::optional<Scene::PickTarget>(readback.targets[id - 1])
                                : std::nullopt);
    }
    readback.targets.clear();
}
} // namespace jhb
