#pragma once

#include "Model.h"

#include <glm/gtc/matrix_transform.hpp>

// std
#include <memory>

namespace jhb {
    struct TransformComponent {
        glm::vec3 translation{};  // (position offset)
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::vec3 rotation {};
        
        // Matrix operation order : scale <- rotate from z axis <- rotate from x axis <- rotate from y axis <- trnaslate
        // Rotation convetion uses tait-bryan angles with axis order 

        glm::mat4 mat4() {
            auto transform = glm::translate(glm::mat4{1.f}, translation);
            transform = glm::rotate(transform, rotation.y, {0.f, 1.f, 0.f});
            transform = glm::rotate(transform, rotation.x, { 0.f, 0.f, 1.f });
            transform = glm::rotate(transform, rotation.z, { 1.f, 0.f, 0.f });
            transform = glm::scale(transform, scale);
            return transform;
        }
    };

    class GameObject {
    public:
        using id_t = unsigned int;

        static GameObject createGameObject() {
            static id_t currentId = 0;
            return GameObject{ currentId++ };
        }

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        GameObject(GameObject&&) = default;
        GameObject& operator=(GameObject&&) = default;

        id_t getId() { return id; }

        std::shared_ptr<Model> model{};
        glm::vec3 color{};
        TransformComponent transform{};

    private:
        GameObject(id_t objId) : id{ objId } {}

        id_t id;
    };
}