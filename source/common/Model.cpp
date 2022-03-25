#include "Model.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "Utils.h"
#include <iostream>

#define TINYGLTF_IMPLEMENTATION

#include <tiny_gltf/tiny_gltf.h>

namespace Examples {

} // namespace Examples

TestApp::MeshBase::MeshBase(vkw::Device &device,
                            tinygltf::Model const &model,
                            tinygltf::Mesh const &mesh, MeshCreateFlags flags) : indexBuffer(device, 1,
                                                                                             VmaAllocationCreateInfo{}),
                                                                                 vertexBuffer(device, 1,
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
                            vertexBuf.at(vertexOffset).normal = glm::vec3(0.0f);
                        }
                        break;
                    }
                    case AttributeType::UV: {
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

void TestApp::MeshBase::bindBuffers(vkw::CommandBuffer &commandBuffer) const {
    commandBuffer.bindVertexBuffer(vertexBuffer, 0, 0);
    commandBuffer.bindIndexBuffer(indexBuffer, 0);
}

void TestApp::MeshBase::drawPrimitive(vkw::CommandBuffer &commandBuffer, int index) const {
    auto &primitive = primitives_.at(index);
    if (primitive.indexCount != 0)
        commandBuffer.drawIndexed(primitive.indexCount, 1, primitive.firstIndex,
                                  primitive.firstVertex);
    else
        commandBuffer.draw(primitive.vertexCount, primitive.firstVertex);
}


void TestApp::Primitive::setDimensions(glm::vec3 min, glm::vec3 max) {
    dimensions.min = min;
    dimensions.max = max;
    dimensions.size = max - min;
    dimensions.center = (min + max) / 2.0f;
    dimensions.radius = glm::distance(min, max) / 2.0f;
}

TestApp::MMesh::MMesh(vkw::Device &device, tinygltf::Model const &model,
                      tinygltf::Node const &node, std::vector<std::reference_wrapper<Material const>> materials,
                      uint32_t maxInstances)
        : MeshBase(device, model, model.meshes[node.mesh]),
          m_materials(std::move(materials)), m_instance_pool(device, maxInstances,
                                                             {VkDescriptorPoolSize{.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount=maxInstances}}),
          m_instance_layout(device, {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}) {

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

void TestApp::MMesh::addNewInstance(
        size_t instanceId,
        vkw::UniformBuffer<InstanceData> const &instanceBuffer) {

    auto &desc = m_instances.emplace(instanceId, vkw::DescriptorSet{m_instance_pool, m_instance_layout}).first->second;
    desc.write(0, instanceBuffer);
}

void TestApp::MMesh::eraseInstance(size_t instanceId) {
    m_instances.erase(instanceId);
}

void TestApp::MMesh::draw(vkw::CommandBuffer &buffer, TestApp::PipelinePool &pool, size_t instanceId) const {
    PipelineDesc desc{};
    desc.material = &m_materials.front().get();
    desc.mesh = this;
    buffer.bindDescriptorSets(pool.pipeline(desc).second, VK_PIPELINE_BIND_POINT_GRAPHICS, m_instances.at(instanceId),
                              1);

    bindBuffers(buffer);

    for (int i = 0; i < primitives_.size(); ++i) {
        desc.material = &m_materials.at(i).get();
        buffer.bindGraphicsPipeline(pool.pipeline(desc).first);
        buffer.bindDescriptorSets(pool.pipeline(desc).second, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_materials.at(i).get().set(), 2);
        drawPrimitive(buffer, i);
    }
}

TestApp::GLTFModel::GLTFModel(vkw::Device &device, RenderEngine::ShaderImporter &loader,
                              std::filesystem::path const &path) :
        renderer_(device), materials(device, 50),
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
        m_shaderPool(loader) {

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
    for (tinygltf::Material &mat: gltfModel.materials) {
        MaterialInfo material;
        if (mat.values.find("baseColorTexture") != mat.values.end()) {
            material.colorMap = &getTexture(
                    gltfModel.textures[mat.values["baseColorTexture"].TextureIndex()]
                            .source);
        }
        material.sampler = &sampler;
#if 0
        // Metallic roughness workflow
        if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
          material.metallicRoughnessTexture = getTexture(
              gltfModel
                  .textures[mat.values["metallicRoughnessTexture"].TextureIndex()]
                  .source);
        }
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
        if (mat.additionalValues.find("normalTexture") !=
            mat.additionalValues.end()) {
          material.normalTexture = getTexture(
              gltfModel
                  .textures[mat.additionalValues["normalTexture"].TextureIndex()]
                  .source);
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
        m_material_infos.push_back(material);
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
        std::vector<std::reference_wrapper<Material const>> primitiveMaterials;
        for (auto &prim: gltfModel.meshes[node.mesh].primitives) {
            auto matIndex = prim.material;
            if (matIndex > -1)
                primitiveMaterials.emplace_back(materials.material(m_material_infos.at(matIndex)));
            else
                primitiveMaterials.emplace_back(
                        materials.material(MaterialInfo{.colorMap = nullptr, .sampler = nullptr})); // default material
        }

        newNode->mesh =
                std::make_unique<MMesh>(renderer_, gltfModel, node, primitiveMaterials, 10000);
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
        MMesh::InstanceData data = node->initialData;

        VmaAllocationCreateInfo createInfo{};
        createInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        auto[buffer, emplaced] = node->instanceBuffers.emplace(
                id, std::pair<MMesh::InstanceData, vkw::UniformBuffer<MMesh::InstanceData>>{data,
                                                                                            vkw::UniformBuffer<MMesh::InstanceData>{
                                                                                                    renderer_,
                                                                                                    createInfo}});

        *buffer->second.second.map() = buffer->second.first;
        buffer->second.second.flush();

        if (node->mesh) {
            node->mesh->addNewInstance(id, buffer->second.second);
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

void TestApp::GLTFModel::drawInstance(vkw::CommandBuffer &recorder, vkw::DescriptorSet const &globalSet,
                                      size_t id) {

    bool flag = false;
    for (auto &node: linearNodes) {
        if (node->mesh) {
            if (!flag) {
                PipelineDesc desc;
                desc.mesh = node->mesh.get();
                desc.material = &materials.material({.colorMap=&textures.front(), .sampler=&sampler});
                recorder.bindDescriptorSets(m_pipelinePool.value().pipeline(desc).second,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS, globalSet,
                                            0);
                flag = false;
            }
            node->mesh->draw(recorder, m_pipelinePool.value(), id);
        }
    }
}

TestApp::GLTFModelInstance::~GLTFModelInstance() {
    if (id_)
        model_->destroyInstance(id_.value());
}

void TestApp::GLTFModelInstance::setRotation(float x, float y, float z) {
    auto rot = glm::mat4(1.0f);
    rot = glm::rotate(rot, glm::radians(x), glm::vec3(1.0f, 0.0f, 0.0f));
    rot = glm::rotate(rot, glm::radians(y), glm::vec3(0.0f, 1.0f, 0.0f));
    rot = glm::rotate(rot, glm::radians(z), glm::vec3(0.0f, 0.0f, 1.0f));

    model_->setRootMatrix(rot, id_.value());
}

TestApp::Material::Material(TestApp::MaterialInfo info, vkw::DescriptorPool &pool,
                            const vkw::DescriptorSetLayout &layout) : m_descriptor(pool, layout), m_layout(layout) {
    VkComponentMapping mapping;
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto &colorMap = *info.colorMap;
    auto &colorMapView = colorMap.getView<vkw::ColorImageView>(pool.device(), colorMap.format(), mapping);

    m_descriptor.write(0, colorMapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *info.sampler);
}

TestApp::PipelinePool::PipelinePool(vkw::Device &device, vkw::DescriptorSetLayout const &globalLayout,
                                    vkw::RenderPass &renderPass, uint32_t subpass, ShaderPool &shaderPool) : m_device(
        device),
                                                                                                             m_globalLayout(
                                                                                                                     globalLayout),
                                                                                                             m_pass(renderPass),
                                                                                                             m_subpass(
                                                                                                                     subpass),
                                                                                                             m_shaderPool(
                                                                                                                     shaderPool) {

}

void TestApp::GLTFModel::setPipelinePool(vkw::RenderPass &pass, uint32_t subpass,
                                         vkw::DescriptorSetLayout const &globalLayout) {
    m_pipelinePool.emplace(renderer_, globalLayout, pass, subpass, m_shaderPool);
}

void TestApp::GLTFModel::setRootMatrix(glm::mat4 transform, size_t id) {
    for (auto &node: rootNodes) {
        auto &buffer = node->instanceBuffers.at(id);
        buffer.first.transform = transform;
        *buffer.second.map() = buffer.first;
        buffer.second.flush();
        buffer.second.unmap();
    }
}

std::pair<std::reference_wrapper<vkw::GraphicsPipeline>, std::reference_wrapper<vkw::PipelineLayout>>
TestApp::PipelinePool::pipeline(TestApp::PipelineDesc desc) {
    if (m_pipeline.contains(desc)) {
        auto &pipe = m_pipeline.at(desc);

        return {pipe.first, pipe.second};
    }
    vkw::PipelineLayout layout{m_device, {desc.mesh->instanceLayout(), desc.material->layout(), m_globalLayout}};
    vkw::GraphicsPipelineCreateInfo createInfo{m_pass, m_subpass, layout};
    createInfo.addVertexShader(m_shaderPool.get().vertexShader("model"));
    createInfo.addFragmentShader(m_shaderPool.get().fragmentShader("model"));
    createInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    createInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    vkw::VertexInputStateCreateInfo<vkw::per_vertex<ModelAttributes, 0>> inputState{};

    createInfo.addVertexInputState(inputState);
    createInfo.addDepthTestState(vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS, true});

    vkw::RasterizationStateCreateInfo rasterizationStateCreateInfo{VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL,
                                                                   VK_CULL_MODE_BACK_BIT,
                                                                   VK_FRONT_FACE_COUNTER_CLOCKWISE};

    createInfo.addRasterizationState(rasterizationStateCreateInfo);

    auto &ret = m_pipeline.emplace(desc, std::pair<vkw::GraphicsPipeline, vkw::PipelineLayout>{
            vkw::GraphicsPipeline{m_device, createInfo}, std::move(layout)}).first->second;

    return {ret.first, ret.second};
}

vkw::VertexShader const &TestApp::ShaderPool::vertexShader(std::string const &name) {
    return m_v_shaders.emplace(name, m_loader.get().loadVertexShader(name)).first->second;
}

vkw::FragmentShader const &TestApp::ShaderPool::fragmentShader(std::string const &name) {
    return m_f_shaders.emplace(name, m_loader.get().loadFragmentShader(name)).first->second;
}