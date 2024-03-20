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
        glm::mat4 mat4() { // model matrix, move in object space to world space
            const float c3 = glm::cos(rotation.z);
            const float s3 = glm::sin(rotation.z);
            const float c2 = glm::cos(rotation.x);
            const float s2 = glm::sin(rotation.x);
            const float c1 = glm::cos(rotation.y);
            const float s1 = glm::sin(rotation.y);
            return glm::mat4{
                {
                    scale.x* (c1* c3 + s1 * s2 * s3),
                        scale.x* (c2* s3),
                        scale.x* (c1* s2* s3 - c3 * s1),
                        0.0f,
                },
                {
                    scale.y * (c3 * s1 * s2 - c1 * s3),
                    scale.y * (c2 * c3),
                    scale.y * (c1 * c3 * s2 + s1 * s3),
                    0.0f,
                },
                {
                    scale.z * (c2 * s1),
                    scale.z * (-s2),
                    scale.z * (c1 * c2),
                    0.0f,
                },
                { translation.x, translation.y, translation.z, 1.0f }
            };
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