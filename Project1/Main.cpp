#ifdef ANDROID
#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <cmath>
#include "JHBApplication.h"
#include "InputController.h"
#include "AndroidAssets.h"
#include "Pipeline.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "VulkanRenderer", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer", __VA_ARGS__)

// -------------------------------------------------------
// JHBApplication은 앱 스레드(appThread)에서 실행된다.
// android_main은 이벤트 처리 전담 루프다.
// ALooper는 android_main 스레드에서만 사용 가능하므로
// Run()에서는 ALooper_pollAll을 호출하지 않는다.
// -------------------------------------------------------
static jhb::JHBApplication* g_app = nullptr;
static std::mutex g_appMutex;
static std::thread g_renderThread;
static std::atomic<bool> g_stopRequested{false};
static void appThreadFunc(struct android_app* app);

static void stopRenderThread()
{
	g_stopRequested.store(true);
	{
		std::lock_guard<std::mutex> lock(g_appMutex);
		if (g_app) g_app->stopAndroidApp();
	}
	// No callback lock while waiting for the renderer to unpublish itself.
	if (g_renderThread.joinable()) g_renderThread.join();
}

static void handleAppCommand(struct android_app* app, int32_t cmd)
{
	switch (cmd) {
	case APP_CMD_INIT_WINDOW:
		if (app->window) {
			stopRenderThread();
			g_stopRequested.store(false);
			g_renderThread = std::thread(appThreadFunc, app);
		}
		break;
	case APP_CMD_TERM_WINDOW:
		// Finish Vulkan work before native_app_glue releases the window.
		stopRenderThread();
		break;
	case APP_CMD_DESTROY:
		stopRenderThread();
		break;
	default:
		break;
	}
}

// 두 손가락 거리 계산 헬퍼
static float getPinchDistance(AInputEvent* event)
{
	float dx = AMotionEvent_getX(event, 1) - AMotionEvent_getX(event, 0);
	float dy = AMotionEvent_getY(event, 1) - AMotionEvent_getY(event, 0);
	return sqrtf(dx * dx + dy * dy);
}

// 터치/핀치 이벤트 → InputController에 전달
// 화면 왼쪽 절반 = 이동, 오른쪽 절반 = 회전, 두 손가락 = 핀치 줌
static int32_t handleInputEvent(struct android_app* app, AInputEvent* event)
{
	if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
	std::lock_guard<std::mutex> appLock(g_appMutex);
	jhb::JHBApplication* jhbApp = g_app;
	if (!jhbApp) return 0;
	auto ic = std::atomic_load(&jhbApp->inputController);
	if (!ic) return 0;

	int32_t action       = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
	int32_t pointerCount = (int32_t)AMotionEvent_getPointerCount(event);

	// ── 두 손가락 핀치 (줌인/줌아웃) ──
	if (action == AMOTION_EVENT_ACTION_POINTER_DOWN && pointerCount == 2) {
		ic->onPinchBegin(getPinchDistance(event));
		return 1;
	}
	if (action == AMOTION_EVENT_ACTION_POINTER_UP && pointerCount == 2) {
		ic->onPinchEnd();
		return 1;
	}
	if (action == AMOTION_EVENT_ACTION_MOVE && pointerCount >= 2) {
		ic->onPinchMove(getPinchDistance(event));
		return 1;
	}

	// ── 단일 손가락: 화면 절반 기준으로 이동 / 회전 분기 ──
	if (pointerCount == 1) {
		float x = AMotionEvent_getX(event, 0);
		float y = AMotionEvent_getY(event, 0);

		// 터치 시작 위치로 왼쪽(이동) / 오른쪽(회전) 결정
		float screenHalfW = app->window
			? (float)ANativeWindow_getWidth(app->window) * 0.5f
			: 540.f; // 창이 없을 경우 fallback

		// 어느 쪽인지 DOWN 시 결정해서 static에 저장
		static bool s_isLeft = false;

		switch (action) {
		case AMOTION_EVENT_ACTION_DOWN:
			s_isLeft = (x < screenHalfW);
			if (s_isLeft) ic->onMoveTouchBegin(x, y);
			else          ic->onTouchBegin(x, y);
			break;
		case AMOTION_EVENT_ACTION_MOVE:
			if (s_isLeft) ic->onMoveTouchMove(x, y);
			else          ic->onTouchMove(x, y);
			break;
		case AMOTION_EVENT_ACTION_UP:
		case AMOTION_EVENT_ACTION_CANCEL:
			if (s_isLeft) ic->onMoveTouchEnd();
			else          ic->onTouchEnd();
			s_isLeft = false;
			break;
		}
	}
	return 1;
}

// -------------------------------------------------------
// 앱 스레드: JHBApplication 초기화 + Run()
// android_main 스레드에서 분리하여 무거운 초기화 중에도
// 이벤트 루프가 살아있도록 하여 ANR을 방지한다.
// -------------------------------------------------------
static void appThreadFunc(struct android_app* app)
{
	std::unique_ptr<jhb::JHBApplication> application;
	try {
		jhb::Window::SetPendingNativeWindow(app->window);
		jhb::setAndroidAssetManager(app->activity->assetManager);

		LOGI("JHBApplication 초기화 시작 (앱 스레드)");
		application = std::make_unique<jhb::JHBApplication>();
		application->setAndroidApp(app);
		{
			std::lock_guard<std::mutex> lock(g_appMutex);
			g_app = application.get();
			if (g_stopRequested.load()) application->stopAndroidApp();
		}

		LOGI("JHBApplication::Run() 시작");
		const auto result = application->Run();
		if (result != VK_SUCCESS)
		{
			LOGE("Renderer failed: VkResult=%d", static_cast<int>(result));
		}

		LOGI("JHBApplication::Run() 종료");
	}
	catch (const std::exception& e) {
		LOGE("앱 스레드 예외: %s", e.what());
	}
	catch (...) {
		LOGE("앱 스레드 알 수 없는 예외");
	}

	{
		std::lock_guard<std::mutex> lock(g_appMutex);
		g_app = nullptr;
	}
	// application is destroyed only after all callbacks have released g_appMutex.
}

void android_main(struct android_app* app)
{
	LOGI("android_main 시작 (이벤트 스레드)");

	app->onAppCmd     = handleAppCommand;
	app->onInputEvent = handleInputEvent;

	// INIT_WINDOW starts the renderer; TERM_WINDOW stops it before releasing the surface.

	// -------------------------------------------------------
	// 이벤트 전담 루프 (android_main 스레드)
	// ALooper는 이 스레드가 소유하므로 여기서만 pollAll 호출.
	// Run()에서는 절대 ALooper_pollAll을 호출하지 않는다.
	// -------------------------------------------------------
	while (!app->destroyRequested) {
		int events;
		android_poll_source* source = nullptr;
		// 1ms 타임아웃: 이벤트 없어도 앱 종료 플래그를 확인하기 위해
		ALooper_pollOnce(1, nullptr, &events, (void**)&source);
		if (source) source->process(app, source);
	}

	stopRenderThread();
	LOGI("android_main 종료");
}

#else

#include <iostream>
#include "JHBApplication.h"

int main() {
	try {
		// swapchain, framebuffer, color, depth attachment are to fixed with window size
		// every time window resize, you must create new swapcahin and others...
		jhb::JHBApplication app;
		const auto result = app.Run();
		if (result != VK_SUCCESS)
		{
			std::cerr << "Renderer failed: VkResult=" << static_cast<int>(result) << std::endl;
			return EXIT_FAILURE;
		}
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#endif
