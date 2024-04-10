#pragma once
#include "Window.h"
#include "GameObject.h"

namespace jhb {
	class InputController
	{
	public:
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
        float moveSpeed{ 3.f };
        float lookSpeed{ 1.5f };

        GameObject& camera;
	};
}

