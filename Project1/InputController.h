#pragma once
#include "Window.h"
#include "GameObject.h"
#ifdef ANDROID
#include <mutex>
#endif

namespace jhb {
	class InputController
	{
	public:
#ifndef ANDROID
        struct KeyMappings {
            int moveLeft = GLFW_KEY_A;
            int moveRight = GLFW_KEY_D;
            int moveForward = GLFW_KEY_W;
            int moveBackward = GLFW_KEY_S;
            int moveUp = GLFW_KEY_E;
            int moveDown = GLFW_KEY_Q;
        int lookLeft = GLFW_KEY_LEFT;
            int lookRight = GLFW_KEY_RIGHT;
            int lookUp = GLFW_KEY_UP;
            int lookDown = GLFW_KEY_DOWN;
        };

        InputController(GLFWwindow& window, GameObject& camera) : camera(camera) {
            glfwSetMouseButtonCallback(&window, jhb::InputController::OnButtonPressed);
        }

        glm::vec3 move(GLFWwindow* window, float dt, GameObject& gameObject);
        static void OnButtonPressed(GLFWwindow* window, int button, int action, int modifier);

        KeyMappings keys{};
#else
        // Android: GLFWwindow 없는 생성자
        InputController() = default;

        // 터치 이벤트 입력 (android_main에서 호출)
        // ── 오른쪽 절반 드래그: 카메라 회전 ──
        void onTouchBegin(float x, float y)
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            touchLastX = x;  touchLastY = y;
            isTouching = true;
        }
        void onTouchMove(float x, float y)
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            if (isTouching && !isPinching) {
                touchDeltaX += x - touchLastX;
                touchDeltaY += y - touchLastY;
                touchLastX = x;  touchLastY = y;
            }
        }
        void onTouchEnd()
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            isTouching = false;
            // 남은 이동량은 렌더 스레드의 move()에서 사용한 뒤 비운다.
        }

        // ── 왼쪽 절반 드래그: 카메라 이동 ──
        void onMoveTouchBegin(float x, float y)
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            moveLastX = x;  moveLastY = y;
            isMoveTouching = true;
        }
        void onMoveTouchMove(float x, float y)
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            if (isMoveTouching && !isPinching) {
                moveDeltaX += x - moveLastX;
                moveDeltaY += y - moveLastY;
                moveLastX = x;  moveLastY = y;
            }
        }
        void onMoveTouchEnd()
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            isMoveTouching = false;
            // 남은 이동량은 렌더 스레드의 move()에서 사용한 뒤 비운다.
        }

        // ── 두 손가락 핀치: 줌인/줌아웃 ──
        void onPinchBegin(float dist)
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            isPinching = true;  pinchLastDist = dist;  pinchDelta = 0.f;
            touchDeltaX = touchDeltaY = 0.f;  // 잔상 방지
            moveDeltaX  = moveDeltaY  = 0.f;
        }
        void onPinchMove(float dist)
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            if (isPinching) {
                pinchDelta   += dist - pinchLastDist;
                pinchLastDist = dist;
            }
        }
        void onPinchEnd()
        {
            std::lock_guard<std::mutex> lock(touchMutex);
            isPinching = false;  pinchDelta = pinchLastDist = 0.f;
        }

        // JHBApplication에서 커서 위치 대신 사용
        float getTouchX() const { std::lock_guard<std::mutex> lock(touchMutex); return touchLastX; }
        float getTouchY() const { std::lock_guard<std::mutex> lock(touchMutex); return touchLastY; }

        glm::vec3 move(float dt, GameObject& gameObject);

    private:
        mutable std::mutex touchMutex;
        // 오른쪽 드래그 (회전)
        float touchLastX  = 0.f, touchLastY  = 0.f;
        float touchDeltaX = 0.f, touchDeltaY = 0.f;
        bool  isTouching  = false;

        // 왼쪽 드래그 (이동)
        float moveLastX   = 0.f, moveLastY   = 0.f;
        float moveDeltaX  = 0.f, moveDeltaY  = 0.f;
        bool  isMoveTouching = false;

        // 두 손가락 핀치
        bool  isPinching    = false;
        float pinchLastDist = 0.f;
        float pinchDelta    = 0.f;

    public:
#endif

        float moveSpeed{ 3.f };
        float lookSpeed{ 1.5f };

#ifndef ANDROID
        GameObject& camera;
#endif
	};
}

