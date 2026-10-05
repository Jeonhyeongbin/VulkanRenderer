#include "InputController.h"
#include "Window.h"
#ifndef ANDROID
#include "External/Imgui/imgui_impl_glfw.h"
#endif
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <limits>

#ifndef ANDROID

glm::vec3 jhb::InputController::move(GLFWwindow* window, float dt, GameObject& gameObject)
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
	const glm::vec3 rightDir = glm::normalize(glm::cross(forwardDir, upDir));

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
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifier);
	auto tmpwindow = (Window*)glfwGetWindowUserPointer(window);
	// Always release camera dragging, including when the cursor entered a UI window.
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
	{
		tmpwindow->SetMouseButtonPress(false);
	}
	if (io.WantCaptureMouse)
	{
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_RIGHT)
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

#else // ANDROID

glm::vec3 jhb::InputController::move(float dt, GameObject& gameObject)
{
	std::lock_guard<std::mutex> lock(touchMutex);
	// ── 오른쪽 드래그: 카메라 회전 ──
	glm::vec3 rotate{ 0 };
	rotate.y -= touchDeltaX * 0.8f; // 좌우 반전
	rotate.x += touchDeltaY * 0.8f;
	if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon())
	{
		gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
	}
	touchDeltaX = touchDeltaY = 0.f;

	// 각도 제한
	gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.f, 1.f);
	gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

	// 현재 방향 벡터 계산
	float yaw   = gameObject.transform.rotation.y;
	float pitch = gameObject.transform.rotation.x;
	glm::vec3 forwardDir;
	forwardDir.x = glm::cos(yaw) * glm::cos(pitch);
	forwardDir.y = glm::sin(pitch);
	forwardDir.z = glm::sin(yaw) * glm::cos(pitch);

	const glm::vec3 upDir{ 0.f, -1.f, 0.f };
	glm::vec3 rightDir = glm::normalize(glm::cross(forwardDir, upDir));

	// ── 왼쪽 드래그: 카메라 이동 ──
	// 수평 델타 → 좌우, 수직 델타 → 앞뒤
	// 손을 뗐어도 아직 사용하지 않은 이동량은 한 번 반영한다.
	const float moveScale = 0.015f;
	gameObject.transform.translation += rightDir   * ( moveDeltaX * moveScale);
	gameObject.transform.translation += forwardDir * (-moveDeltaY * moveScale); // 위로 드래그 = 앞으로
	moveDeltaX = moveDeltaY = 0.f;

	// ── 두 손가락 핀치: 줌인/줌아웃 ──
	if (std::abs(pinchDelta) > std::numeric_limits<float>::epsilon())
	{
		gameObject.transform.translation += forwardDir * (pinchDelta * 0.01f);
		pinchDelta = 0.f;
	}

	return forwardDir;
}

#endif // ANDROID


