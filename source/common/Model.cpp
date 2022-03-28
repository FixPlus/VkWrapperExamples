#include "Model.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "Utils.h"
#include <iostream>
#include <RenderEngine/RecordingState.h>

#define TINYGLTF_IMPLEMENTATION

#include <tiny_gltf/tiny_gltf.h>

static const vkw::VertexInputStateCreateInfo<vkw::per_vertex<TestApp::ModelAttributes, 0>> ModelVertexInputState{};

TestApp::MeshBase::MeshBase(vkw::Device &device, tinygltf::Model const &model, tinygltf::Mesh const &mesh,
                            std::vector<ModelMaterial> const &materials, MeshCreateFlags flags) : indexBuffer(device, 1,
                                                                                                              VmaAllocationCreateInfo{}),
                                                                                                  vertexBuffer(device,
                                                                                                               1,
                                                                                                               VmaAllocationCreateInfo{}),
                                                                                                  name_(mesh.name) {

    std::vector<AttributeType> attrMapping{};

    for (int i = 0; i < static_cast<int>(AttributeType::LAST); ++i)
        attrMapping.push_back(static_cast<AttributeType>(i));

    size_t totalIndexCount = 0, totalVertexCount = 0;

    std::vector<ModelAttributes> vertexBuf;
    std::vector<uint32_t> indexBuf;
    size_t perVertexSize = 0;

    for (auto &attr: attrMapping) {
        switch (attr) {

            case AttributeType::NORMAL:
                perVertexSize += sizeof(glm::vec3);
                break;
            case AttributeType::UV:
                perVertexSize += sizeof(glm::vec2);
                break;
            case AttributeType::POSITION:
            case AttributeType::COLOR:
            case AttributeType::JOINT:
            case AttributeType::WEIGHT:
                perVertexSize += sizeof(glm::vec4);
                break;
            case AttributeType::LAST:
                break;
        }
    }

    for (auto &primitive: mesh.primitives) {


        auto &evalPrimitive = primitives_.emplace_back();
        evalPrimitive.material = &materials.at(primitive.material);

        evalPrimitive.firstVertex = totalVertexCount;
        evalPrimitive.firstIndex = totalIndexCount;

        evalPrimitive.vertexCount =
                model.accessors[primitive.attributes.find("POSITION")->second].count;

        auto indexRef = primitive.indices;
        if (indexRef >= 0)
            evalPrimitive.indexCount = model.accessors[indexRef].count;
        else
            evalPrimitive.indexCount = 0;

        totalIndexCount += evalPrimitive.indexCount;
        totalVertexCount += evalPrimitive.vertexCount;

        vertexBuf.resize(totalVertexCount);
        indexBuf.reserve(totalIndexCount);

        const float *bufferPos = nullptr;
        const float *bufferNormals = nullptr;
        const float *bufferTexCoords = nullptr;
        const float *bufferColors = nullptr;
        const float *bufferTangents = nullptr;

        int numColorComponents;

        const tinygltf::Accessor &posAccessor =
                model.accessors[primitive.attributes.find("POSITION")->second];
        const tinygltf::BufferView &posView =
                model.bufferViews[posAccessor.bufferView];
        bufferPos = reinterpret_cast<const float *>(
                &(model.buffers[posView.buffer]
                        .data[posAccessor.byteOffset + posView.byteOffset]));
        auto posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1],
                                posAccessor.minValues[2]);
        auto posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1],
                                posAccessor.maxValues[2]);

        evalPrimitive.setDimensions(posMin, posMax);
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
            const tinygltf::Accessor &normAccessor =
                    model.accessors[primitive.attributes.find("NORMAL")->second];
            const tinygltf::BufferView &normView =
                    model.bufferViews[normAccessor.bufferView];
            bufferNormals = reinterpret_cast<const float *>(
                    &(model.buffers[normView.buffer]
                            .data[normAccessor.byteOffset + normView.byteOffset]));
        }

        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
            const tinygltf::Accessor &uvAccessor =
                    model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
            const tinygltf::BufferView &uvView =
                    model.bufferViews[uvAccessor.bufferView];
            bufferTexCoords = reinterpret_cast<const float *>(
                    &(model.buffers[uvView.buffer]
                            .data[uvAccessor.byteOffset + uvView.byteOffset]));
        }

        if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
            const tinygltf::Accessor &colorAccessor =
                    model.accessors[primitive.attributes.find("COLOR_0")->second];
            const tinygltf::BufferView &colorView =
                    model.bufferViews[colorAccessor.bufferView];
            // Color buffer are either of type vec3 or vec4
            numColorComponents =
                    colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
            bufferColors = reinterpret_cast<const float *>(
                    &(model.buffers[colorView.buffer]
                            .data[colorAccessor.byteOffset + colorView.byteOffset]));
        }

        if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
            const tinygltf::Accessor &colorAccessor =
                    model.accessors[primitive.attributes.find("COLOR_0")->second];
            const tinygltf::BufferView &colorView =
                    model.bufferViews[colorAccessor.bufferView];
            // Color buffer are either of type vec3 or vec4
            numColorComponents =
                    colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
            bufferColors = reinterpret_cast<const float *>(
                    &(model.buffers[colorView.buffer]
                            .data[colorAccessor.byteOffset + colorView.byteOffset]));
        }

        if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
        {
            const tinygltf::Accessor &tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
            const tinygltf::BufferView &tangentView = model.bufferViews[tangentAccessor.bufferView];
            bufferTangents = reinterpret_cast<const float *>(&(model.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
        }

        for (size_t i = 0; i < evalPrimitive.vertexCount; ++i) {
            size_t vertexOffset = (evalPrimitive.firstVertex + i);
            size_t localOffset = 0;
            for (auto &attr: attrMapping) {
                switch (attr) {
                    case AttributeType::POSITION: {
                        glm::vec4 pos = glm::vec4(glm::make_vec3(bufferPos + i * 3), 1.0f);
                        vertexBuf.at(vertexOffset).pos = pos;
                        break;
                    }
                    case AttributeType::NORMAL: {
                        if (bufferNormals) {
                            glm::vec3 normal = glm::vec3(glm::make_vec3(bufferNormals + i * 3));
                            vertexBuf.at(vertexOffset).normal = normal;
                        } else {
                            vertexBuf.at(vertexOffset).normal = glm::normalize(glm::vec3(1.0f));
                        }
                        break;
                    }
                    case AttributeType::TANGENT: {
                        if (bufferTangents) {
                            glm::vec3 tangent = glm::vec3(glm::make_vec3(bufferTangents + i * 3));
                            vertexBuf.at(vertexOffset).tangent = tangent;
                        } else {
                            vertexBuf.at(vertexOffset).tangent = glm::normalize(glm::vec3(1.0f));
                        }
                        break;
                    }                    case AttributeType::UV: {
                        if (bufferTexCoords) {
                            glm::vec2 uv = glm::vec2(glm::make_vec2(bufferTexCoords + i * 2));
                            vertexBuf.at(vertexOffset).uv = uv;
                        } else {
                            vertexBuf.at(vertexOffset).uv = glm::vec2(0.0f);
                        }
                        break;
                    }
                    case AttributeType::COLOR: {
                        if (bufferColors) {
                            glm::vec4 color = glm::vec4(0.0f);
                            color = numColorComponents == 4
                                    ? glm::make_vec4(bufferColors + i * 4)
                                    : glm::vec4(glm::make_vec3(bufferColors + i * 3), 0.0f);
                            vertexBuf.at(vertexOffset).color = color;
                        } else {
                            vertexBuf.at(vertexOffset).color = glm::vec4(0.0f);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        if (evalPrimitive.indexCount > 0) {
            const tinygltf::Accessor &accessor = model.accessors[primitive.indices];
            const tinygltf::BufferView &bufferView =
                    model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

            switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    auto *buf = new uint32_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset],
                           accessor.count * sizeof(uint32_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuf.push_back(buf[index]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                    auto *buf = new uint16_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset],
                           accessor.count * sizeof(uint16_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuf.push_back(buf[index]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                    auto *buf = new uint8_t[accessor.count];
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset],
                           accessor.count * sizeof(uint8_t));
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuf.push_back(buf[index]);
                    }
                    break;
                }
                default:
                    throw std::runtime_error(
                            "[MESH][ERROR] loaded gltf model has invalid index type");
            }
        }
    }

    // TODO: implement vertex pretransformation here

    vertexBuffer = createStaticBuffer<vkw::VertexBuffer<ModelAttributes>, ModelAttributes>(device, vertexBuf.begin(),
                                                                                           vertexBuf.end());

    indexBuffer = createStaticBuffer<vkw::IndexBuffer<VK_INDEX_TYPE_UINT32>, uint32_t>(device, indexBuf.begin(),
                                                                                       indexBuf.end());

}

void TestApp::MeshBase::bindBuffers(RenderEngine::GraphicsRecordingState &recorder) const {
    recorder.commands().bindVertexBuffer(vertexBuffer, 0, 0);
    recorder.commands().bindIndexBuffer(indexBuffer, 0);
}

void TestApp::MeshBase::drawPrimitive(RenderEngine::GraphicsRecordingState &recorder, int index) const {
    auto &primitive = primitives_.at(index);
    recorder.setMaterial(*primitive.material);
    recorder.bindPipeline();

    if (primitive.indexCount != 0)
        recorder.commands().drawIndexed(primitive.indexCount, 1, primitive.firstIndex,
                                        primitive.firstVertex);
    else
        recorder.commands().draw(primitive.vertexCount, primitive.firstVertex);
}


void TestApp::Primitive::setDimensions(glm::vec3 min, glm::vec3 max) {
    dimensions.min = min;
    dimensions.max = max;
    dimensions.size = max - min;
    dimensions.center = (min + max) / 2.0f;
    dimensions.radius = glm::distance(min, max) / 2.0f;
}

TestApp::MMesh::MMesh(vkw::Device &device, tinygltf::Model const &model,
                      tinygltf::Node const &node, const std::vector<ModelMaterial> &materials,
                      uint32_t maxInstances)
        : MeshBase(device, model, model.meshes[node.mesh], materials) {

}

void TestApp::GLTFModel::loadImages(tinygltf::Model &gltfModel) {
    RenderEngine::TextureLoader loader(renderer_, "");

    for (auto &image: gltfModel.images) {
        unsigned char *imageBuf = image.image.data();
        size_t bufferSize = image.width * image.height * 4;
        bool deleteBuffer = false;
        if (image.component == 3) {
            imageBuf = new unsigned char[bufferSize];
            unsigned char *rgba = imageBuf;
            unsigned char *rgb = &image.image[0];
            for (size_t i = 0; i < image.width * image.height; ++i) {
                for (int32_t j = 0; j < 3; ++j) {
                    rgba[j] = rgb[j];
                }
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }


        textures.push_back(loader.loadTexture(imageBuf, image.width, image.height));
    }
}

TestApp::ModelGeometry &
TestApp::MMesh::addNewInstance(size_t instanceId, ModelGeometryLayout &layout, vkw::Device &device) {
    return m_instances.emplace(instanceId, ModelGeometry{device, layout}).first->second;
}

void TestApp::MMesh::eraseInstance(size_t instanceId) {
    m_instances.erase(instanceId);
}

void TestApp::MMesh::draw(RenderEngine::GraphicsRecordingState &recorder, size_t instanceId) const {
    recorder.setGeometry(m_instances.at(instanceId));

    bindBuffers(recorder);

    for (int i = 0; i < primitives_.size(); ++i) {
        drawPrimitive(recorder, i);
    }
}

const TestApp::ModelGeometry &TestApp::MMesh::instance(size_t id) const {
    return m_instances.at(id);
}

TestApp::ModelGeometry &TestApp::MMesh::instance(size_t id) {
    return m_instances.at(id);
}

TestApp::GLTFModel::GLTFModel(vkw::Device &device, DefaultTexturePool& pool,
                              std::filesystem::path const &path) :
        renderer_(device),
        sampler(device, VkSamplerCreateInfo{.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext=nullptr,
                .magFilter=VK_FILTER_LINEAR,
                .minFilter=VK_FILTER_LINEAR,
                .mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT,

                .minLod=0.0f,
                .maxLod=1.0f,
        }),
        materialLayout(device),
        geometryLayout(device),
        m_defaultTexturePool(pool){

    if (!path.has_extension())
        throw std::runtime_error("[MODEL][ERROR] " + path.generic_string() +
                                 " file has no extension.");

    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF gltfContext;

    std::string error, warning;

    bool fileLoaded;

    if (path.extension() == ".gltf")
        fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning,
                                                   path.generic_string());
    else if (path.extension() == ".glb")
        fileLoaded = gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning,
                                                    path.generic_string());
    else
        throw std::runtime_error(
                "[MODEL][ERROR] " + path.generic_string() +
                " file has incorrect extension. Supported extensions are: gltf, glb");

    if (!fileLoaded)
        throw std::runtime_error("[GLTF][ERROR] failed to load model from file " +
                                 path.generic_string() + ". " + error);

    if (!warning.empty())
        std::cout << "[GLTF][WARNING]: " << warning << std::endl;

    loadImages(gltfModel);

    loadMaterials(gltfModel);

    const tinygltf::Scene &scene =
            gltfModel
                    .scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

    for (int nodeIndex: scene.nodes) {
        const tinygltf::Node node = gltfModel.nodes[nodeIndex];
        loadNode(gltfModel, nullptr, node, nodeIndex);
    }
}

void TestApp::GLTFModel::loadMaterials(tinygltf::Model &gltfModel) {
    uint32_t counter = 0;
    for (tinygltf::Material &mat: gltfModel.materials) {
        MaterialInfo material;
        if (mat.values.find("baseColorTexture") != mat.values.end()) {
            material.colorMap = &getTexture(
                    gltfModel.textures[mat.values["baseColorTexture"].TextureIndex()]
                            .source);
        }
        material.sampler = &sampler;

        if (mat.additionalValues.find("normalTexture") !=
            mat.additionalValues.end()) {
            material.normalMap = &getTexture(
                    gltfModel
                            .textures[mat.additionalValues["normalTexture"].TextureIndex()]
                            .source);
        }

        if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
            material.metallicRoughnessMap = &getTexture(
                    gltfModel
                            .textures[mat.values["metallicRoughnessTexture"].TextureIndex()]
                            .source);
        }
#if 0
        // Metallic roughness workflow

        if (mat.values.find("roughnessFactor") != mat.values.end()) {
          material.materialSettings.roughnessFactor =
              static_cast<float>(mat.values["roughnessFactor"].Factor());
        }
        if (mat.values.find("metallicFactor") != mat.values.end()) {
          material.materialSettings.metallicFactor =
              static_cast<float>(mat.values["metallicFactor"].Factor());
        }
        if (mat.values.find("baseColorFactor") != mat.values.end()) {
          material.materialSettings.baseColorFactor =
              glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
        }

        if (mat.additionalValues.find("emissiveTexture") !=
            mat.additionalValues.end()) {
          material.emissiveTexture = getTexture(
              gltfModel
                  .textures[mat.additionalValues["emissiveTexture"].TextureIndex()]
                  .source);
        }
        if (mat.additionalValues.find("occlusionTexture") !=
            mat.additionalValues.end()) {
          material.occlusionTexture = getTexture(
              gltfModel
                  .textures[mat.additionalValues["occlusionTexture"].TextureIndex()]
                  .source);
        }
        if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end()) {
          tinygltf::Parameter param = mat.additionalValues["alphaMode"];
          if (param.string_value == "BLEND") {
            material.alphaMode = MMaterial::ALPHAMODE_BLEND;
          }
          if (param.string_value == "MASK") {
            material.alphaMode = MMaterial::ALPHAMODE_MASK;
          }
        }
        if (mat.additionalValues.find("alphaCutoff") !=
            mat.additionalValues.end()) {
          material.materialSettings.alphaCutoff =
              static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
        }

        material.sampler = &sampler;

        material.compileDescriptors();
#endif
        materials.emplace_back(renderer_.get(), m_defaultTexturePool, materialLayout, material);
    }
}

void TestApp::GLTFModel::loadNode(tinygltf::Model &gltfModel,
                                  std::shared_ptr<MNode> parent,
                                  const tinygltf::Node &node,
                                  uint32_t nodeIndex) {

    std::shared_ptr<MNode> newNode = std::make_shared<MNode>();
    newNode->index = nodeIndex;
    newNode->parent = parent;
    newNode->name = node.name;

    // Generate local node matrix

    // if node transform stored in uniform matrix - decompose it
    if (node.matrix.size() == 16) {
        glm::vec4 dummyPer;
        glm::mat4 uniformMatrix = glm::make_mat4x4(node.matrix.data());
        newNode->initialData.transform = uniformMatrix;
    } else {
        glm::mat4 transform{1.0f};

        glm::vec3 translation = glm::vec3(0.0f);
        if (node.translation.size() == 3) {
            translation = glm::make_vec3(node.translation.data());
        }
        transform = glm::translate(transform, translation);

        glm::mat4 rotation = glm::mat4(1.0f);
        if (node.rotation.size() == 4) {
            glm::quat q = glm::make_quat(node.rotation.data());
            rotation = glm::mat4(q);
        }
        transform = transform * rotation;

        glm::vec3 scale = glm::vec3(1.0f);
        if (node.scale.size() == 3) {
            scale = glm::make_vec3(node.scale.data());
        }
        transform = glm::scale(transform, scale);
        newNode->initialData.transform = transform;
    }

    // Node with children
    if (!node.children.empty()) {
        for (int i: node.children) {
            loadNode(gltfModel, newNode, gltfModel.nodes[i], i);
        }
    }

    if (node.mesh > -1) {
        newNode->mesh =
                std::make_unique<MMesh>(renderer_, gltfModel, node, materials, 10000);
    }

    if (parent) {
        parent->children.push_back(newNode);
    } else {
        rootNodes.push_back(newNode);
    }
    linearNodes.push_back(newNode);
}

TestApp::GLTFModelInstance TestApp::GLTFModel::createNewInstance() {
    size_t id;
    if (!freeIDs.empty()) {
        id = freeIDs.top();
        freeIDs.pop();
    } else {
        id = instanceCount;
    }

    instanceCount++;

    for (auto &node: linearNodes) {
        auto &data = node->instanceBuffers.emplace(id, node->initialData).first->second;
        if (node->mesh) {
            auto &geom = node->mesh->addNewInstance(id, geometryLayout, renderer_);
            geom.data = data;
            geom.update();
        }
    }

    return {this, id};
}

void TestApp::GLTFModel::destroyInstance(size_t id) {
    instanceCount--;
    if (id != instanceCount) {
        freeIDs.push(id);
    }

    for (auto &node: linearNodes) {
        node->instanceBuffers.erase(id);
        if (node->mesh)
            node->mesh->eraseInstance(id);
    }
}

void
TestApp::GLTFModel::drawInstance(RenderEngine::GraphicsRecordingState &recorder,
                                 size_t id) {

    for (auto &node: linearNodes) {
        if (node->mesh) {
            node->mesh->draw(recorder, id);
        }
    }
}

TestApp::GLTFModelInstance::~GLTFModelInstance() {
    if (id_)
        model_->destroyInstance(id_.value());
}

void TestApp::GLTFModelInstance::update() {
    auto rot = glm::mat4(1.0f);

    rot = glm::rotate(rot, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rot = glm::rotate(rot, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rot = glm::rotate(rot, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    rot = glm::scale(rot, scale);

    model_->setRootMatrix(rot, id_.value());
}

static void updateMatricesRecursively(TestApp::ModelGeometry::Data parentData, TestApp::MNode *node, size_t id) {
    if (!node)
        return;
    auto data = node->instanceBuffers.at(id);
    data.transform = parentData.transform * data.transform;
    if (node->mesh) {
        auto &geom = node->mesh->instance(id);
        geom.data = data;
        geom.update();
    }
    for (auto &child: node->children) {
        updateMatricesRecursively(data, child.get(), id);
    }
}

void TestApp::GLTFModel::setRootMatrix(glm::mat4 transform, size_t id) {
    for (auto &node: rootNodes) {
        auto &buffer = node->instanceBuffers.at(id);
        buffer.transform = transform;

        TestApp::ModelGeometry::Data init{};
        init.transform = glm::mat4{1.0f};
        updateMatricesRecursively(init, node.get(), id);
    }
}

TestApp::ModelMaterialLayout::ModelMaterialLayout(vkw::Device &device) :
        RenderEngine::MaterialLayout(device,
                                     RenderEngine::MaterialLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="pbr", .setBindings={
                                             vkw::DescriptorSetLayoutBinding{0,
                                                                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                             vkw::DescriptorSetLayoutBinding{1,
                                                                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                             vkw::DescriptorSetLayoutBinding{2,
                                                                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}}, .rasterizationState=vkw::RasterizationStateCreateInfo(
                                             VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL,
                                             VK_CULL_MODE_BACK_BIT), .depthTestState=vkw::DepthTestStateCreateInfo(
                                             VK_COMPARE_OP_LESS, VK_TRUE), .maxMaterials=50}) {

}

TestApp::ModelMaterial::ModelMaterial(vkw::Device &device, DefaultTexturePool& pool, ModelMaterialLayout &layout, MaterialInfo info)
        : RenderEngine::Material(layout), m_info(info) {
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    auto* colorMap = info.colorMap ? info.colorMap : &pool.colorMap();
    auto* normalMap = info.normalMap ? info.normalMap : &pool.colorMap();
    auto* mrMap = info.metallicRoughnessMap ? info.metallicRoughnessMap : &pool.metallicRoughnessMap();

    set().write(0, colorMap->getView<vkw::ColorImageView>(device, colorMap->format(), mapping),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *info.sampler);
    set().write(1, normalMap->getView<vkw::ColorImageView>(device, normalMap->format(), mapping),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *info.sampler);
    set().write(2, mrMap->getView<vkw::ColorImageView>(device, mrMap->format(), mapping),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *info.sampler);
}

TestApp::ModelGeometryLayout::ModelGeometryLayout(vkw::Device &device) :
        RenderEngine::GeometryLayout(device,
                                     RenderEngine::GeometryLayout::CreateInfo{.vertexInputState=&ModelVertexInputState, .substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="model", .setBindings={
                                             vkw::DescriptorSetLayoutBinding{0,
                                                                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}}, .maxGeometries=1000}) {

}

TestApp::ModelGeometry::ModelGeometry(vkw::Device &device, TestApp::ModelGeometryLayout &layout)
        : RenderEngine::Geometry(layout), m_ubo(device,
                                                VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
          m_mapped(m_ubo.map()) {
    set().write(0, m_ubo);
}

void TestApp::ModelGeometry::update() {
    *m_mapped = data;
    m_ubo.flush();
}

TestApp::DefaultTexturePool::DefaultTexturePool(vkw::Device &device, uint32_t textureDim):
        m_colorMap(device.getAllocator(), VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},VK_FORMAT_R8G8B8A8_UNORM, textureDim, textureDim, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        m_normalMap(device.getAllocator(), VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},VK_FORMAT_R8G8B8A8_UNORM, textureDim, textureDim, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        m_metallicRoughnessMap(device.getAllocator(), VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},VK_FORMAT_R8G8B8A8_UNORM, textureDim, textureDim, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)

{
    vkw::Buffer<uint32_t> stageBuffer{device, textureDim * textureDim, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU,.requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}};

    auto* textureData = stageBuffer.map();

    for(int i = 0; i < textureDim; ++i)
        for(int j = 0; j < textureDim; ++j)
            textureData[i * textureDim + j] = (i < textureDim / 2 && j < textureDim / 2) || (i > textureDim / 2 && j > textureDim / 2) ? 0xFFFF00FF : 0xFF000000;

    stageBuffer.flush();

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = m_colorMap;
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = 1;
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = 0;

    VkImageMemoryBarrier transitLayout2{};
    transitLayout2.image = m_colorMap;
    transitLayout2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout2.pNext = nullptr;
    transitLayout2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitLayout2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout2.subresourceRange.baseArrayLayer = 0;
    transitLayout2.subresourceRange.baseMipLevel = 0;
    transitLayout2.subresourceRange.layerCount = 1;
    transitLayout2.subresourceRange.levelCount = 1;
    transitLayout2.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout2.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    auto transferQueue = device.getTransferQueue();
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, transferQueue->familyIndex()};
    auto transferCommand = vkw::PrimaryCommandBuffer{commandPool};
    transferCommand.begin(0);
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout1});
    VkBufferImageCopy bufferCopy{};
    bufferCopy.imageExtent = {static_cast<uint32_t>(textureDim), static_cast<uint32_t>(textureDim), 1};
    bufferCopy.imageSubresource.mipLevel = 0;
    bufferCopy.imageSubresource.layerCount = 1;
    bufferCopy.imageSubresource.baseArrayLayer = 0;
    bufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    transferCommand.copyBufferToImage(stageBuffer, m_colorMap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {bufferCopy});

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout2});
    transferCommand.end();

    transferQueue->submit(transferCommand);
    transferQueue->waitIdle();

    for(int i = 0; i < textureDim; ++i)
        for(int j = 0; j < textureDim; ++j)
            textureData[i * textureDim + j] = 0x0;
    stageBuffer.flush();
    transitLayout1.image = m_normalMap;
    transitLayout2.image = m_normalMap;

    transferCommand.begin(0);
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout1});

    transferCommand.copyBufferToImage(stageBuffer, m_normalMap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {bufferCopy});

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout2});
    transferCommand.end();

    transferQueue->submit(transferCommand);
    transferQueue->waitIdle();


    for(int i = 0; i < textureDim; ++i)
        for(int j = 0; j < textureDim; ++j)
            textureData[i * textureDim + j] = 0xFFFFFFFF;

    stageBuffer.flush();
    transitLayout1.image = m_metallicRoughnessMap;
    transitLayout2.image = m_metallicRoughnessMap;

    transferCommand.begin(0);
    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout1});

    transferCommand.copyBufferToImage(stageBuffer, m_metallicRoughnessMap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {bufferCopy});

    transferCommand.imageMemoryBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       {transitLayout2});
    transferCommand.end();

    transferQueue->submit(transferCommand);
    transferQueue->waitIdle();

}
