#include "InputController.h"
#include "External/Imgui/imgui_impl_glfw.h"

glm::vec3  jhb::InputController::move(GLFWwindow* window, float dt, GameObject& gameObject)
{
	const float cameraSpeed = 0.1f;
	glm::vec3 rotate{ 0 };
	if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
	if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
	if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
	if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

	if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon())
	{
		gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
	}

	// for not upside down
	auto limitAngle = 1.f;
	gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -limitAngle, limitAngle);
	gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

	float yaw = gameObject.transform.rotation.y;
	float pitch = gameObject.transform.rotation.x;
	glm::vec3 forwardDir(0);
	forwardDir.x = glm::cos(yaw) * glm::cos(pitch);
	forwardDir.y = glm::sin(pitch);
	forwardDir.z = glm::sin(yaw) * glm::cos(pitch);
	const glm::vec3 upDir = { 0, -1, 0 };
	const glm::vec3 rightDir =glm::normalize(glm::cross(forwardDir, upDir));


	glm::vec3 moveDir{0.f};
	if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir * cameraSpeed;
	if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir * cameraSpeed;
	if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir * cameraSpeed;
	if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir * cameraSpeed;
	if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir * cameraSpeed;
	if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir * cameraSpeed;

	if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
	{
		gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
	}
	return forwardDir;
}

void jhb::InputController::OnButtonPressed(GLFWwindow* window, int button, int action, int modifier)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse)
	{
		ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifier);
		return;
	}

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


