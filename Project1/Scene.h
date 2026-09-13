#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace jhb
{
namespace rg
{
struct Texture;
}
class Device;
class Model;
struct Vertex;

// Owns loaded model references and light data. Pipeline/descriptor setup stays with rendering.
class Scene
{
  public:
    struct PointLightData
    {
        glm::vec3 position{0.0f};
        glm::vec3 color{1.0f};
        float intensity{1.0f};
        float radius{0.1f};
    };

    std::vector<PointLightData> &pointLights()
    {
        return pointLights_;
    }

    const std::vector<PointLightData> &pointLights() const
    {
        return pointLights_;
    }

    explicit Scene(Device &device);
    ~Scene();

    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;

    std::shared_ptr<Model> loadGLTF(const std::string &filename);
    std::shared_ptr<Model> loadOBJ(const std::string &filename);
    std::shared_ptr<Model> createCube();
    std::shared_ptr<Model> createBrdfTriangle();
    std::shared_ptr<Model> environmentCube();
    // Non-owning graph import; Scene retains the loaded cube and its sampler.
    rg::Texture environmentTexture();

  private:
    std::shared_ptr<Model> createMesh(const std::vector<Vertex> &vertices,
                                      const std::vector<uint32_t> &indices);

    Device &device_;
    std::vector<std::shared_ptr<Model>> models_;
    std::shared_ptr<Model> environmentCube_;
    std::vector<PointLightData> pointLights_;
};
} // namespace jhb
