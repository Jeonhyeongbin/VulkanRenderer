#pragma once
#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "Window.h"
#include "Device.h"
#include "SwapChain.h"
#include "Model.h"
#include "GameObject.h"
#include "Renderer.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "FrameInfo.h"
#include "RenderGraph/RenderGraph.h"
#include "PickingPass.h"
#include "Scene.h"

#ifdef ANDROID
#include <android_native_app_glue.h>
#include <atomic>
#endif

#include <stdint.h>
#include <chrono>
#include <array>

namespace jhb
{
class JHBApplication
{
  public:
    JHBApplication();
    ~JHBApplication();

    JHBApplication(const JHBApplication &) = delete;
    JHBApplication(JHBApplication &&) = delete;
    JHBApplication &operator=(const JHBApplication &) = delete;

    VkResult Run();

#ifdef ANDROID
    void setAndroidApp(struct android_app *app)
    {
        androidApp = app;
        // Window는 android_main에서 SetPendingNativeWindow → Window 생성자 경로로
        // 이미 설정됐다. 같은 포인터로 다시 SetNativeWindow를 호출하면
        // framebufferResized = true 가 재설정되어 첫 프레임에서 불필요한
        // 스왑체인 재생성이 일어나므로, 창 포인터가 실제로 변경된 경우에만 호출한다.
        if (window.GetNativeWindow() != app->window)
        {
            window.SetNativeWindow(app->window);
        }
    }

    // 앱 스레드(Run())에 종료를 요청한다.
    // android_main 이벤트 스레드에서 APP_CMD_DESTROY/APP_CMD_TERM_WINDOW 수신 시 호출.
    void stopAndroidApp()
    {
        androidAppRunning.store(false);
    }

    // Atomic shared_ptr access keeps the controller alive during event callbacks.
    std::shared_ptr<class InputController> inputController;
#endif

  private:
    void init();
    void collectSceneModels();
    void prepareIBL();
    void recordIBL(VkCommandBuffer commandBuffer);
    void prepareDescriptors(const rg::Pipeline &pipeline);
    void releaseDescriptors();
    void updateViewBuffer(int frameIndex);
    void ensureFrameGraph();
    VkResult recordFrame(VkCommandBuffer commandBuffer);
    void consumePicking(int frameIndex);

  private:
    // init top to bottom
    Window window{800, 600, "TriangleApp!"};
    Device device{window};
    std::unique_ptr<class Scene> GlobalScene;

    Renderer renderer{window, device};

    std::vector<std::unique_ptr<Buffer>> uboBuffers{};

  private:
    std::vector<rg::DescriptorSet> viewDescriptorSets;
    std::vector<rg::DescriptorSet> materialDescriptorSets;
    std::vector<std::shared_ptr<Model>> sceneModels;

    struct IBLTextures
    {
        rg::TextureId environment{}, brdfLut{}, irradiance{}, prefilter{};
    };

    // Persistent owner; frameGraph only imports these textures, including after resize.
    std::unique_ptr<rg::RenderGraph> iblGraph;
    IBLTextures iblTextures;
    IBLTextures frameIBLTextures;
    bool iblRecorded{false};
    std::unique_ptr<rg::RenderGraph> frameGraph;
    std::array<std::unique_ptr<class GBufferPass>, SwapChain::MAX_FRAMES_IN_FLIGHT> gbufferPasses;
    std::array<std::unique_ptr<class DeferredLightingPass>, SwapChain::MAX_FRAMES_IN_FLIGHT>
        lightingPasses;
    std::array<rg::TextureId, SwapChain::MAX_FRAMES_IN_FLIGHT> shadowCubes{}, shadowDepths{};
    std::unique_ptr<class PresentPass> presentPass;
    uint64_t graphGeneration{0};
    bool descriptorsReady{false};

    struct PickingReadback
    {
        std::unique_ptr<Buffer> buffer;
        rg::BufferId graphBuffer{};
        std::vector<Scene::PickTarget> targets;
        uint64_t request{0}, generation{0};
        bool pending{false};
    };

    std::array<PickingReadback, SwapChain::MAX_FRAMES_IN_FLIGHT> pickingReadbacks;
    std::array<PickingPass::Resources, SwapChain::MAX_FRAMES_IN_FLIGHT> pickingImages;
    uint64_t latestPickRequest{0};

#ifndef ANDROID
    std::unique_ptr<class ImGuiPass> imguiPass;
    std::unique_ptr<class ImGuiContext> imguiContext;
    float uiRoughness{0.1f}, uiMetallic{0.1f};
#endif

  private:
#ifdef ANDROID
    struct android_app *androidApp = nullptr;
    // android_main 이벤트 스레드에서 stopAndroidApp()으로 false를 설정하면
    // Run()이 루프를 종료한다. 두 스레드에서 접근하므로 atomic 사용.
    std::atomic<bool> androidAppRunning{true};
#endif
};
} // namespace jhb
