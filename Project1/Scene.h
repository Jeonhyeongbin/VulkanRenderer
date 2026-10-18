#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Image.h"
#include <array>
#include <optional>

namespace jhb
{
namespace rg
{
struct Texture;
}
class Device;
class Model;
struct Material;
struct Vertex;

// Owns loaded model references and light data. Pipeline/descriptor setup stays with rendering.
class Scene
{
  public:
    // std430: four vec4s followed by eight uints (96-byte array stride).
    struct GpuMaterial
    {
        glm::vec4 baseColor;
        glm::vec4 emissive;
        glm::vec4 metallicRoughnessAlpha; // metallic, roughness, alpha-mask enabled, cutoff
        glm::vec4 normalOcclusion;       // normal scale, AO strength, double-sided, unused
        uint32_t baseColorTexture;
        uint32_t normalTexture;
        uint32_t occlusionTexture;
        uint32_t emissiveTexture;
        uint32_t metallicRoughnessTexture;
        uint32_t padding[3]{};
    };
    static_assert(sizeof(GpuMaterial) == 96);

    VkResult collectMaterialResources(const std::vector<std::shared_ptr<Model>> &models,
                                     std::vector<VkDescriptorImageInfo> &images,
                                     std::vector<GpuMaterial> &materials);
    struct PickTarget
    {
        uint32_t objectId;
        std::weak_ptr<Model> model;
    };

    std::vector<PickTarget> pickingTargets(const std::shared_ptr<Model> &model) const;
    void select(std::optional<PickTarget> target);
    std::optional<uint32_t> selectedObject() const;
    // Borrowed only for immediate editing; Model owns the instance transform.
    glm::mat4 *selectedTransform();

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
    void loadDefaultScene();
    std::array<VkDescriptorImageInfo, 5> materialImages(const Model &model,
                                                        const Material &material) const;
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
    std::optional<PickTarget> selection_;
    std::vector<std::shared_ptr<Model>> models_;
    std::shared_ptr<Model> environmentCube_;
    std::vector<PointLightData> pointLights_;
    std::vector<rg::Texture> defaultTextures_;
};
} // namespace jhb
