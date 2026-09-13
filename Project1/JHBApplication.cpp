#include "JHBApplication.h"
#include "GBufferPass.h"
#include "PresentPass.h"
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
    uboBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    viewDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    GlobalScene = std::make_unique<Scene>(device);

    init();
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

void JHBApplication::Run()
{
    auto viewerObject = GameObject::createGameObject();
    viewerObject.transform.translation.x = 0.0f;             // 좌우 정중앙
    viewerObject.transform.translation.y = -12.0f;           // 위쪽 (Y음수 = 위)
    viewerObject.transform.translation.z = 0.0f;             // 앞뒤 정중앙
    viewerObject.transform.rotation.x = glm::radians(35.0f); // 35도 아래를 향함
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
        glfwGetCursorPos(&window.GetGLFWwindow(), &x, &y);
#endif
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime =
            std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime)
                .count();
        currentTime = newTime;

        auto commandBuffer = renderer.beginFrame();
        if (commandBuffer ==
            nullptr) // begine frame return null pointer if swap chain need recreated
        {
            continue;
        }

        int frameIndex = renderer.getFrameIndex();
        if (!pickingPhase(static_cast<int>(x), static_cast<int>(y)))
        {
            // camera controll phase
            if (window.objectId == 0 && window.GetMousePressed())
            {
                window.mouseMove(x, y, frameTime, viewerObject);
            }
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
        renderer.endFrame();
    }

    // Run() 루프 종료 후 dangling pointer 방지
    // (cameraController는 이 함수가 반환되면 소멸됨)
#ifdef ANDROID
    inputController.store(nullptr);
#endif
    vkDeviceWaitIdle(device.getLogicalDevice());
}

void JHBApplication::init()
{
    for (int frame = 0; frame < SwapChain::MAX_FRAMES_IN_FLIGHT; ++frame)
    {
        uboBuffers[frame] = std::make_unique<Buffer>(device, sizeof(GlobalUbo), 1,
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uboBuffers[frame]->map();
    }

    collectSceneModels();
    GlobalScene->pointLights().push_back({{0.0f, -10.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, 5.0f, 0.1f});
#ifndef ANDROID
    imguiPass = std::make_unique<ImGuiPass>();
    // Step 1 previews G-buffer albedo directly; this is not final lighting/tonemapping.
    imguiContext = std::make_unique<ImGuiContext>(
        device, VK_FORMAT_R16G16B16A16_SFLOAT,
        static_cast<uint32_t>(renderer.GetSwapChain().getImageCount()));
#endif
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
            std::array<VkDescriptorImageInfo, 5> images = {
                model->getTexture(material.baseColorTextureIndex).descriptor,
                model->getTexture(material.normalTextureIndex).descriptor,
                model->getTexture(material.occlusionTextureIndex).descriptor,
                model->getTexture(material.emissiveTextureIndex).descriptor,
                model->getTexture(material.metallicRoughnessTextureIndex).descriptor};
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
    for (auto &pass : gbufferPasses)
    {
        pass.reset();
    }
    frameGraph = std::make_unique<rg::RenderGraph>(device, SwapChain::MAX_FRAMES_IN_FLIGHT);
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
    }
    presentPass = std::make_unique<PresentPass>(*frameGraph,
                                                renderer.GetSwapChain().getSwapChainImageFormat());
    graphGeneration = generation;
}

void JHBApplication::recordFrame(VkCommandBuffer commandBuffer)
{
    ensureFrameGraph();
    const int frameIndex = renderer.getFrameIndex();
    auto &graph = *frameGraph;
    graph.newFrame(frameIndex);
    graph.clear();
    gbufferPasses[frameIndex]->addPass(
        [this, frameIndex](VkCommandBuffer cmd, const rg::RenderPass &pass,
                           const rg::GraphResources &resources)
        {
            const auto layout = resources.pipeline(pass.pipelineHandle).layout;
            for (const auto &model : sceneModels)
            {
                model->drawGraphGeometry(cmd, layout, frameIndex);
            }
        });
    const auto previewColor = gbufferPasses[frameIndex]->output().albedo;
#ifndef ANDROID
    imguiContext->configureTarget(graph.resources().texture(previewColor).texture.image.desc.format,
                                  static_cast<uint32_t>(renderer.GetSwapChain().getImageCount()));
    imguiContext->beginFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 300.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("settings", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::TextUnformatted("G-buffer albedo preview (lighting pending)");
    ImGui::Text("Models: %d", static_cast<int>(sceneModels.size()));
    ImGui::SliderFloat("roughness", &uiRoughness, 0.1f, 1.0f);
    ImGui::SliderFloat("metallic", &uiMetallic, 0.1f, 1.0f);
    ImGui::End();
    imguiContext->endFrame();
    imguiPass->addPass(graph, previewColor, imguiContext->drawFunction());
#endif
    presentPass->addPass(previewColor);
    graph.prepare();
    prepareDescriptors(graph.resources().pipeline(graph.passes().front().pipelineHandle));

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
    vk_sync::imagePipelineBarrier(
        commandBuffer, presentImage.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        vk_sync::AccessType::ColorAttachmentWrite, vk_sync::AccessType::Present, false);
}

bool JHBApplication::pickingPhase(int x, int y)
{
    // TODO(migration): declare PickingPass when requested, then copy its ID pixel
    // after graph rendering and consume readback only after the submission fence.
    // Selection manipulation below only consumes an already resolved window.objectId.

    // picking only apply to pbrobjects
    if (window.objectId > 0)
    {
        auto &pickedObject = GameObjectManager::GetSingleton().gameObjects[window.objectId - 1];
        {
            if (pickedObject.model && pickedObject.getId() >= 2)
            {
                // should transfer rotation axis to object space;
                uint32_t obj_first_id = pickedObject.model->firstid;
                pickedObject.transform.rotation =
                    glm::rotate(glm::mat4{1.f}, (float)((px - x) * (0.001)), glm::vec3{1, 0, 0}) *
                    glm::vec4(pickedObject.transform.rotation, 1);
                px = x, py = y;

                std::vector<glm::vec3> tmplist(pickedObject.model->instanceCount);
                for (int i = 0; i < pickedObject.model->instanceCount; i++)
                {
                    tmplist[i] = GameObjectManager::GetSingleton()
                                     .gameObjects[obj_first_id + i]
                                     .transform.translation;
                }

                std::vector<glm::vec3> tmprot(pickedObject.model->instanceCount);
                for (int i = 0; i < pickedObject.model->instanceCount; i++)
                {
                    tmprot[i] = GameObjectManager::GetSingleton()
                                    .gameObjects[obj_first_id + i]
                                    .transform.rotation;
                }

                tmprot[pickedObject.getId() - pickedObject.model->firstid] =
                    pickedObject.transform.rotation;
                {
                    pickedObject.model->updateInstanceBuffer(pickedObject.model->instanceCount,
                                                             tmplist, tmprot);
                }
            }
        }
        return true;
    }

    // if not objectpicking state then store currnet mouse coordinate for next pickingphase
    px = x;
    py = y;

    // and return false boolean for processing camera phase
    return false;
}
} // namespace jhb
