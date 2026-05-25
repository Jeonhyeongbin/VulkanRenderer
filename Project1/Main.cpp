#ifdef ANDROID
#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/log.h>
#include "JHBApplication.h"
#include "InputController.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "VulkanRenderer", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer", __VA_ARGS__)

// Android 앱 커맨드 핸들러 (Window 생성/소멸 이벤트 처리)
static jhb::JHBApplication* g_app = nullptr;
static jhb::InputController* g_inputController = nullptr;

static void handleAppCommand(struct android_app* app, int32_t cmd)
{
	switch (cmd) {
	case APP_CMD_INIT_WINDOW:
		if (g_app && app->window) {
			g_app->setAndroidApp(app);
		}
		break;
	case APP_CMD_TERM_WINDOW:
		break;
	case APP_CMD_DESTROY:
		break;
	}
}

// OnButtonPressed 대체: 터치 이벤트 → InputController에 전달
static int32_t handleInputEvent(struct android_app* app, AInputEvent* event)
{
	if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
	if (g_inputController == nullptr) return 0;

	int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
	float x = AMotionEvent_getX(event, 0);
	float y = AMotionEvent_getY(event, 0);

	switch (action) {
	case AMOTION_EVENT_ACTION_DOWN:
		// GLFW OnButtonPressed PRESS에 해당
		g_inputController->onTouchBegin(x, y);
		break;
	case AMOTION_EVENT_ACTION_MOVE:
		g_inputController->onTouchMove(x, y);
		break;
	case AMOTION_EVENT_ACTION_UP:
	case AMOTION_EVENT_ACTION_CANCEL:
		// GLFW OnButtonPressed RELEASE에 해당
		g_inputController->onTouchEnd();
		break;
	}
	return 1; // 이벤트 소비했음을 알림
}

void android_main(struct android_app* app)
{
	LOGI("android_main started");

	try {
		jhb::JHBApplication application;
		g_app = &application;

		app->onAppCmd     = handleAppCommand;
	app->onInputEvent = handleInputEvent;

		// Window가 준비될 때까지 대기
		while (app->window == nullptr) {
			int events;
			android_poll_source* source;
			ALooper_pollAll(0, nullptr, &events, (void**)&source);
			if (source) source->process(app, source);
		}

		// Native Window를 Application에 전달
		application.setAndroidApp(app);

		// Run() 안에서 InputController가 생성된 후 포인터가 등록됨
		// → handleInputEvent에서 터치 이벤트를 전달할 수 있게 됨
		g_inputController = application.inputController;

		application.Run();

		g_app = nullptr;
	}
	catch (const std::exception& e) {
		LOGE("Exception: %s", e.what());
	}
}

#else

#include <iostream>
#include "JHBApplication.h"

int main() {
	try {
		// swapchain, framebuffer, color, depth attachment are to fixed with window size
		// every time window resize, you must create new swapcahin and others...
		jhb::JHBApplication app;
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#endif