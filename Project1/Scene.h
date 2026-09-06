#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

namespace jhb
{
class Device;
class Model;
struct Vertex;

// Owns loaded model references. Pipeline/descriptor setup stays with rendering.
class Scene
{
  public:
    explicit Scene(Device &device);
    ~Scene();

    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;

    std::shared_ptr<Model> loadGLTF(const std::string &filename);
    std::shared_ptr<Model> loadOBJ(const std::string &filename);
    std::shared_ptr<Model> createCube();
    std::shared_ptr<Model> createBrdfTriangle();
    std::shared_ptr<Model> environmentCube();

  private:
    std::shared_ptr<Model> createMesh(const std::vector<Vertex> &vertices,
                                      const std::vector<uint32_t> &indices);

    Device &device_;
    std::vector<std::shared_ptr<Model>> models_;
    std::shared_ptr<Model> environmentCube_;
};
} // namespace jhb
