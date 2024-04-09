#include "InputController.h"

void jhb::InputController::moveInPlaneXZ(GLFWwindow* window, float dt, GameObject& gameObject)
{
	glm::vec3 rotate{0};
	if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
	if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
	if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
	if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

	if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon())
	{
		gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
	}

	// for not upside down
	gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
	gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

	float yaw = gameObject.transform.rotation.y;
	const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
	const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
	const glm::vec3 upDir = glm::normalize(glm::cross(forwardDir, rightDir));

	glm::vec3 moveDir{0.f};
	if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
	if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
	if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
	if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
	if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
	if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

	if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
	{
		gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
	}
}

void jhb::InputController::OnButtonPressed(GLFWwindow* window, int button, int action, int modifier)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT|| button == GLFW_MOUSE_BUTTON_LEFT)
	{
		auto tmpwindow = (Window*)glfwGetWindowUserPointer(window);
		if (action == GLFW_PRESS)
		{
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			tmpwindow->SetMouseCursorPose(x, y);
			tmpwindow->SetMouseButtonPress(true);
		}
		else if (action == GLFW_RELEASE)
		{
			tmpwindow->SetMouseButtonPress(false);
		}
	}
}


