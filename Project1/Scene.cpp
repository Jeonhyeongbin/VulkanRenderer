#include "Scene.h"
#include "Model.h"
#include "Device.h"
#ifdef ANDROID
#include "AndroidAssets.h"
#endif

namespace jhb
{
Scene::Scene(Device &device) : device_(device)
{
}

Scene::~Scene() = default;

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
        model->loadTextures(glTFInput);
        model->loadImages(glTFInput, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);
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
