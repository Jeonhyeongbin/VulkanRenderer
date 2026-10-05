#include "Scene.h"
#include "Model.h"
#include "Device.h"
#include "GameObjectManager.h"
#include <limits>
#ifdef ANDROID
#include "AndroidAssets.h"
#endif

namespace jhb
{
Scene::Scene(Device &device) : device_(device)
{
}

std::vector<Scene::PickTarget> Scene::pickingTargets(const std::shared_ptr<Model> &model) const
{
    std::vector<PickTarget> targets;
    bool hasSelectableInstance = false;
    const auto &objects = GameObjectManager::GetSingleton().gameObjects;
    for (uint32_t instance = 0; instance < model->instanceCount; ++instance)
    {
        const uint64_t id = uint64_t(model->firstid) + instance;
        if (id > (std::numeric_limits<uint32_t>::max)())
        {
            throw std::logic_error("Picking instance object ID overflow");
        }
        const auto found = objects.find(static_cast<uint32_t>(id));
        if (found == objects.end() || found->second.model != model)
        {
            throw std::logic_error("Picking requires an object mapping for every model instance");
        }
        targets.push_back({static_cast<uint32_t>(id), model});
        hasSelectableInstance |= found->second.selectable;
    }
    // 인스턴스 번호와 목록 위치는 유지한다. 개별 선택 여부는 selectedObject에서 확인한다.
    if (!hasSelectableInstance)
    {
        targets.clear();
    }
    return targets;
}

void Scene::select(std::optional<PickTarget> target)
{
    selection_ = std::move(target);
    if (!selectedObject())
    {
        selection_.reset();
    }
}

std::optional<uint32_t> Scene::selectedObject() const
{
    if (selection_)
    {
        const auto model = selection_->model.lock();
        const auto &objects = GameObjectManager::GetSingleton().gameObjects;
        const auto found = objects.find(selection_->objectId);
        if (model && found != objects.end() && found->second.model == model &&
            found->second.selectable)
        {
            return selection_->objectId;
        }
    }
    return std::nullopt;
}

void Scene::rotateSelectedObject(float angleRadians)
{
    const auto selected = selectedObject();
    if (!selected || angleRadians == 0.0f)
    {
        return;
    }

    auto &object = GameObjectManager::GetSingleton().gameObjects.at(*selected);
    auto &model = object.model;
    if (*selected < model->firstid ||
        *selected - model->firstid >= model->instanceData.size())
    {
        return;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> rotations;
    for (const auto &instance : model->instanceData)
    {
        positions.push_back(instance.pos);
        rotations.push_back(instance.rot);
    }

    const auto instanceIndex = *selected - model->firstid;
    // 이 헬멧은 glTF에서 X축 90도 회전된다.
    // 기존 쉐이더의 rot.x(로컬 Z 회전)가 이 모델의 월드 수직축에 해당한다.
    rotations[instanceIndex].x += angleRadians;
    model->updateInstanceBuffer(model->instanceCount, positions, rotations);
    object.transform.rotation = model->instanceData[instanceIndex].rot;
}

Scene::~Scene()
{
    // Remove only this Scene's registry references, so its models die before Device.
    auto &objects = GameObjectManager::GetSingleton().gameObjects;
    for (auto it = objects.begin(); it != objects.end();)
    {
        if (std::find(models_.begin(), models_.end(), it->second.model) != models_.end())
        {
            it = objects.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto &texture : defaultTextures_)
    {
        device_.destroyTexture(texture);
    }
}

void Scene::loadDefaultScene()
{
    if (defaultTextures_.empty())
    {
        const std::array<VkClearColorValue, 4> colors = {
            VkClearColorValue{{1, 1, 1, 1}}, VkClearColorValue{{0.5f, 0.5f, 1, 1}},
            VkClearColorValue{{1, 1, 1, 1}}, VkClearColorValue{{1, 1, 1, 1}}};
        rg::ImageDesc desc;
        desc.format = VK_FORMAT_R8G8B8A8_UNORM;
        desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        defaultTextures_.reserve(colors.size());
        for (const auto &color : colors)
        {
            defaultTextures_.push_back(device_.createTexture(desc, "material/defaultTexture"));
            auto &texture = defaultTextures_.back();
            auto cmd = device_.beginSingleTimeCommands();
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            device_.transitionImageLayout(cmd, texture.image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);
            vkCmdClearColorImage(cmd, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &color, 1, &range);
            device_.transitionImageLayout(cmd, texture.image.image,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);
            device_.endSingleTimeCommands(cmd);
        }
    }
    auto &objects = GameObjectManager::GetSingleton().gameObjects;
    if (!objects.empty())
    {
        return;
    }
    auto object = GameObject::createGameObject();
    object.model = loadGLTF("Models/DamagedHelmet/DamagedHelmet.gltf");
    object.selectable = true;
    object.model->updateInstanceBuffer(1, {glm::vec3(0)}, {glm::vec3(0)});
    object.model->firstid = object.getId();
    GameObjectManager::GetSingleton().AddGameObject(std::move(object));

    auto sponza = GameObject::createGameObject();
    sponza.model = loadGLTF("Models/sponza/Sponza.gltf");
    sponza.selectable = false;
    // Keep the helmet at the origin, above the atrium floor.
    sponza.model->rootModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, -2, 0));
    sponza.model->inverseRootModelMatrix = glm::inverse(sponza.model->rootModelMatrix);
    sponza.model->updateInstanceBuffer(1, {glm::vec3(0)}, {glm::vec3(0)});
    sponza.model->firstid = sponza.getId();
    GameObjectManager::GetSingleton().AddGameObject(std::move(sponza));
}

std::array<VkDescriptorImageInfo, 5> Scene::materialImages(const Model &model,
                                                           const Material &material) const
{
    const auto image = [&](uint32_t index, size_t defaultTextureIndex)
    {
        if (index < model.textures.size())
        {
            const auto source = model.textures[index].imageIndex;
            if (source >= 0 && static_cast<size_t>(source) < model.images.size())
            {
                const auto &value = model.images[source];
                if (value.view && value.sampler &&
                    value.descriptor.imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                {
                    return value.descriptor;
                }
            }
        }
        return defaultTextures_.at(defaultTextureIndex).descriptorInfo;
    };
    return {image(material.baseColorTextureIndex, 0), image(material.normalTextureIndex, 1),
            image(material.occlusionTextureIndex, 0), image(material.emissiveTextureIndex, 2),
            image(material.metallicRoughnessTextureIndex, 3)};
}

std::shared_ptr<Model> Scene::loadGLTF(const std::string &filename)
{
    tinygltf::Model glTFInput;
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;

#ifdef ANDROID
    jhb::setupGltfAndroidCallbacks(gltfContext);
#endif

    const bool binary = filename.size() >= 4 && filename.substr(filename.size() - 4) == ".glb";
    const bool fileLoaded =
        binary ? gltfContext.LoadBinaryFromFile(&glTFInput, &error, &warning, filename)
               : gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

    size_t pos = filename.find_last_of("/\\");

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    std::shared_ptr<Model> model = std::make_shared<Model>(device_);

    model->path = pos == std::string::npos ? "." : filename.substr(0, pos);

    if (fileLoaded)
    {
        model->loadMaterials(glTFInput);
        model->materials.emplace_back(); // glTF primitives may omit their material.
        model->loadTextures(glTFInput);
        model->loadImages(glTFInput, VK_SAMPLER_ADDRESS_MODE_REPEAT);
        const auto sceneIndex = glTFInput.defaultScene < 0 ? 0 : glTFInput.defaultScene;
        if (static_cast<size_t>(sceneIndex) >= glTFInput.scenes.size())
        {
            throw std::runtime_error("glTF has no valid scene: " + filename);
        }
        const tinygltf::Scene &scene = glTFInput.scenes.at(sceneIndex);
        for (size_t i = 0; i < scene.nodes.size(); i++)
        {
            const tinygltf::Node node = glTFInput.nodes.at(scene.nodes[i]);
            model->loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
        }

        // Retain legacy tangent generation for the forward PBR path. Its w stays
        // zero so the G-buffer can use the more robust derivative-based fallback.
        if (!model->hasTangent)
        {
            for (size_t i = 0; i + 2 < indexBuffer.size(); i += 3)
            {
                Vertex &vertex1 = vertexBuffer[indexBuffer[i]];
                Vertex &vertex2 = vertexBuffer[indexBuffer[i + 1]];
                Vertex &vertex3 = vertexBuffer[indexBuffer[i + 2]];
                model->calculateTangent(vertex1.uv, vertex2.uv, vertex3.uv, vertex1.position,
                                        vertex2.position, vertex3.position, vertex1.tangent);
                model->calculateTangent(vertex2.uv, vertex1.uv, vertex3.uv, vertex2.position,
                                        vertex1.position, vertex3.position, vertex2.tangent);
                model->calculateTangent(vertex3.uv, vertex2.uv, vertex1.uv, vertex3.position,
                                        vertex2.position, vertex1.position, vertex3.tangent);
            }
        }
    }
    else
    {
        throw std::runtime_error("Failed to load glTF: " + filename + "\n" + error);
    }

    if (vertexBuffer.empty() || indexBuffer.empty())
    {
        throw std::runtime_error("glTF scene contains no indexed geometry: " + filename);
    }
    model->createVertexBuffer(vertexBuffer);
    model->createIndexBuffer(indexBuffer);
    model->createObjectSphere(vertexBuffer);
    models_.push_back(model);
    return model;
}

std::shared_ptr<Model> Scene::loadOBJ(const std::string &filename)
{
    auto model = std::make_shared<Model>(device_);
    model->loadModel(filename);
    model->uploadGeometry();
    models_.push_back(model);
    return model;
}

std::shared_ptr<Model> Scene::createMesh(const std::vector<Vertex> &vertices,
                                         const std::vector<uint32_t> &indices)
{
    auto model = std::make_shared<Model>(device_);
    model->createVertexBuffer(vertices);
    model->createIndexBuffer(indices);
    model->createObjectSphere(vertices);
    models_.push_back(model);
    return model;
}

std::shared_ptr<Model> Scene::createCube()
{
    std::vector<Vertex> vertices = {
        {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},  {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},   {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},

        {{.5f, -.5f, -.5f}, {.8f, .8f, .5f}},   {{.5f, .5f, .5f}, {.8f, .8f, .5f}},
        {{.5f, -.5f, .5f}, {.8f, .8f, .5f}},    {{.5f, .5f, -.5f}, {.8f, .8f, .5f}},

        {{-.5f, -.5f, -.5f}, {.9f, .6f, .5f}},  {{.5f, -.5f, .5f}, {.9f, .6f, .5f}},
        {{-.5f, -.5f, .5f}, {.9f, .6f, .5f}},   {{.5f, -.5f, -.5f}, {.9f, .6f, .5f}},

        {{-.5f, .5f, -.5f}, {.8f, .5f, .5f}},   {{.5f, .5f, .5f}, {.8f, .5f, .5f}},
        {{-.5f, .5f, .5f}, {.8f, .5f, .5f}},    {{.5f, .5f, -.5f}, {.8f, .5f, .5f}},

        {{-.5f, -.5f, 0.5f}, {.5f, .5f, .8f}},  {{.5f, .5f, 0.5f}, {.5f, .5f, .8f}},
        {{-.5f, .5f, 0.5f}, {.5f, .5f, .8f}},   {{.5f, -.5f, 0.5f}, {.5f, .5f, .8f}},

        {{-.5f, -.5f, -0.5f}, {.5f, .8f, .5f}}, {{.5f, .5f, -0.5f}, {.5f, .8f, .5f}},
        {{-.5f, .5f, -0.5f}, {.5f, .8f, .5f}},  {{.5f, -.5f, -0.5f}, {.5f, .8f, .5f}},
    };

    std::vector<uint32_t> indices = {0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,
                                     8,  9,  10, 8,  11, 9,  12, 13, 14, 12, 15, 13,
                                     16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21};

    return createMesh(vertices, indices);
}

std::shared_ptr<Model> Scene::createBrdfTriangle()
{
    std::vector<Vertex> vertices = {
        {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
        {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},

    };

    std::vector<uint32_t> indices = {0, 1, 2};

    return createMesh(vertices, indices);
}

std::shared_ptr<Model> Scene::environmentCube()
{
    if (!environmentCube_)
    {
        auto cube = createCube();
        cube->getTexture(0).loadKTXTexture(device_, "Texture/pisa_cube.ktx",
                                           VK_IMAGE_VIEW_TYPE_CUBE, 6);
        environmentCube_ = std::move(cube);
    }
    return environmentCube_;
}

rg::Texture Scene::environmentTexture()
{
    const auto &source = environmentCube()->getTexture(0);
    if (!source.image || !source.view || !source.sampler || !source.width ||
        source.width != source.height || source.layerCount != 6 || !source.mipLevels ||
        source.format == VK_FORMAT_UNDEFINED ||
        source.imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        throw std::runtime_error("Environment cube upload did not produce a sampled cube texture");
    }
    rg::Texture texture;
    texture.image.image = source.image;
    texture.image.imageView = source.view;
    texture.image.desc.width = source.width;
    texture.image.desc.height = source.height;
    texture.image.desc.mipLevels = source.mipLevels;
    texture.image.desc.arrayLayers = source.layerCount;
    texture.image.desc.format = source.format;
    texture.image.desc.imageType = rg::ImageType::CubeMap;
    texture.image.desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    texture.sampler = source.sampler;
    texture.descriptorInfo = source.descriptor;
    return texture;
}
} // namespace jhb
