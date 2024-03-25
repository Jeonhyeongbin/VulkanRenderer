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

        // 오일러 앵글, 아핀변환(종합 매트릭스..), 
          // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
        glm::mat4 mat4(); // model matrix, move in object space to world space
        glm::mat3 normalMatrix();
    };

    class GameObject {
    public:
        using id_t = unsigned int;
        using Map = std::unordered_map<id_t, GameObject>;

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