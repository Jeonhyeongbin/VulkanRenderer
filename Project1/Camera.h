#pragma once
#include <stdlib.h>

#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

namespace jhb {
	class Camera
	{
	public:
		Camera(float fovy);

		void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);

		void setPerspectiveProjection(float aspect, float near, float far);

		void setViewDirection(glm::vec3 cameraPosition, glm::vec3 cameraDirection, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
		void setViewTarget(glm::vec3 cameraPosition, glm::vec3 target, glm::vec3 up = glm::vec3{ 0.f, -1.f, 0.f });
		void setViewYXZ(glm::vec3 position, glm::vec3 rotation);
		float getfovy() { return fovy; }
		void setfovy(float _fovy) { fovy = _fovy; }

		const glm::mat4& getProjection() const { return projectionMatrix; }
		const glm::mat4& getView() const { return viewMatrix; }
		const glm::mat4& getInverseView() const { return inverseViewMatrix; }
	private:
		glm::mat4 inverseViewMatrix{ 1.f };
		glm::mat4 projectionMatrix{ 1.f }; // camera space to canonical view volume;
		glm::mat4 viewMatrix{1.f}; // objects in world space move to camera space;
		float fovy; // radianse
	};
}

