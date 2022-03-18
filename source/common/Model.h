#ifndef RENDERAPITEST_MODEL_H
#define RENDERAPITEST_MODEL_H

#define TINYGLTF_NO_STB_IMAGE_WRITE

#include <filesystem>
#include <glm/detail/type_quat.hpp>
#include <glm/glm.hpp>
#include <stack>
#include <stdexcept>
#include <tiny_gltf/tiny_gltf.h>
#include <vkw/VertexBuffer.hpp>
#include <CommandBuffer.hpp>
#include <Shader.hpp>
#include <AssetImport.h>
#include <Pipeline.hpp>
#include <DescriptorSet.hpp>
#include <DescriptorPool.hpp>
#include <Sampler.hpp>

namespace TestApp {

    enum class AttributeType {
        POSITION = 0, // 4f
        NORMAL = 1,   // 3f
        UV = 2,       // 2f
        COLOR = 3,    // 4f
        JOINT = 4,    // 4f
        WEIGHT = 5,   // 4f
        LAST = 6
    };

    struct ModelAttributes : public vkw::AttributeBase<vkw::VertexAttributeType::VEC4F, vkw::VertexAttributeType::VEC3F,
            vkw::VertexAttributeType::VEC2F, vkw::VertexAttributeType::VEC4F, vkw::VertexAttributeType::VEC4F, vkw::VertexAttributeType::VEC4F> {
        glm::vec4 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 color;
        glm::vec4 joint;
        glm::vec4 weight;
    };

    struct Primitive {
        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t firstVertex;
        uint32_t vertexCount;

        struct BoundingBox {
            glm::vec3 min = glm::vec3(FLT_MAX);
            glm::vec3 max = glm::vec3(-FLT_MAX);
            glm::vec3 size;
            glm::vec3 center;
            float radius;
        } dimensions;

        void setDimensions(glm::vec3 min, glm::vec3 max);
    };

    enum MeshCreateFlagsBits {
        MESH_PRE_TRANSFORM_VERTICES = 0x00000001,
        MESH_PRE_MULTIPLY_COLORS = 0x00000002,
        MESH_FLIP_Y = 0x00000004,
    };

    using MeshCreateFlags = uint32_t;

    class MeshBase {
        vkw::VertexBuffer<ModelAttributes> vertexBuffer;
        vkw::IndexBuffer<VK_INDEX_TYPE_UINT32> indexBuffer;

    protected:
        std::vector<Primitive> primitives_;
        std::string name_;

    public:
        MeshBase(vkw::Device &device, tinygltf::Model const &model,
                 tinygltf::Mesh const &mesh, MeshCreateFlags flags = 0);

        void bindBuffers(vkw::CommandBuffer &commandBuffer) const;

        /** bindBuffers must be called before executing this method */
        void drawPrimitive(vkw::CommandBuffer &commandBuffer, int index) const;

        size_t primitiveCount() const { return primitives_.size(); };

        /** useful for camera frustum culling and collision detection. */
        Primitive::BoundingBox getPrimitiveBoundingBox(int index) const {
            return primitives_.at(index).dimensions;
        };

        std::string_view name() const { return name_; }

        virtual ~MeshBase() = default;
    };

    class GLTFModel;

#if 0
    class TexturePool{
    public:
        TexturePool(TextureLoader& loader);

        vkw::ColorImage2D const& texture(std::string name);

    private:
        std::map<std::string, vkw::ColorImage2D> m_textures;
        std::reference_wrapper<TextureLoader> m_loader;
    };
#endif

    class ShaderPool {
    public:
        ShaderPool(ShaderLoader &loader) : m_loader(loader) {}

        vkw::VertexShader const &vertexShader(std::string const &name);

        vkw::FragmentShader const &fragmentShader(std::string const &name);


    private:
        std::map<std::string, vkw::VertexShader> m_v_shaders{};
        std::map<std::string, vkw::FragmentShader> m_f_shaders{};

        std::reference_wrapper<ShaderLoader> m_loader;
    };

    struct MaterialInfo {
        vkw::ColorImage2D *colorMap;
        vkw::Sampler const *sampler;

        auto operator<=>(MaterialInfo const &another) const = default;
    };

    class Material {
    public:
        Material(MaterialInfo info, vkw::DescriptorPool &pool, vkw::DescriptorSetLayout const &layout);

        vkw::DescriptorSet const &set() const {
            return m_descriptor;
        };

        vkw::DescriptorSetLayout const &layout() const {
            return m_layout;
        };
    private:

        vkw::DescriptorSet m_descriptor;
        std::reference_wrapper<vkw::DescriptorSetLayout const> m_layout;
    };

    class MaterialPool {
    public:
        MaterialPool(vkw::Device &device, uint32_t maxMaterials) :
                m_material_descriptor_pool(device, maxMaterials,
                                           {VkDescriptorPoolSize{.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount=maxMaterials}}),
                m_descriptor_layout(device,
                                    {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}) {
        }

        Material const &material(MaterialInfo info) {
            if (m_materials.contains(info))
                return m_materials.at(info);
            else
                return m_materials.emplace(info, Material{info, m_material_descriptor_pool,
                                                          m_descriptor_layout}).first->second;
        }

        vkw::DescriptorSetLayout const &layout() const {
            return m_descriptor_layout;
        };
    private:
        std::map<MaterialInfo, Material> m_materials{};
        vkw::DescriptorPool m_material_descriptor_pool;
        vkw::DescriptorSetLayout m_descriptor_layout;
    };

    class MMesh;

    struct PipelineDesc {
        Material const *material;
        MMesh const *mesh;

        auto operator<=>(PipelineDesc const &another) const = default;
    };

    class PipelinePool {
    public:
        explicit PipelinePool(vkw::Device &device, vkw::DescriptorSetLayout const &globalLayout,
                              vkw::RenderPass &renderPass, uint32_t subpass, ShaderPool &shaderPool);

        std::pair<std::reference_wrapper<vkw::GraphicsPipeline>, std::reference_wrapper<vkw::PipelineLayout>>
        pipeline(PipelineDesc desc);

    private:
        std::reference_wrapper<vkw::RenderPass> m_pass;
        std::reference_wrapper<ShaderPool> m_shaderPool;
        uint32_t m_subpass;
        std::reference_wrapper<vkw::Device> m_device;
        std::reference_wrapper<vkw::DescriptorSetLayout const> m_globalLayout;
        std::map<PipelineDesc, std::pair<vkw::GraphicsPipeline, vkw::PipelineLayout>> m_pipeline;

    };

    class MMesh : public MeshBase {
        std::vector<std::reference_wrapper<Material const>> m_materials;
        vkw::DescriptorPool m_instance_pool;
        std::map<size_t, vkw::DescriptorSet> m_instances{};
        vkw::DescriptorSetLayout m_instance_layout;
    public:
        struct InstanceData {
            glm::mat4 transform = glm::mat4(1.0f);
        };

        MMesh(vkw::Device &device, tinygltf::Model const &model,
              tinygltf::Node const &node, std::vector<std::reference_wrapper<Material const>> materials,
              uint32_t maxInstances);


        vkw::DescriptorSet const &instanceSet(size_t instanceId) const {
            return m_instances.at(instanceId);
        }

        vkw::DescriptorSetLayout const &instanceLayout() const {
            return m_instance_layout;
        }

        void draw(vkw::CommandBuffer &buffer, PipelinePool &pool, size_t instanceId) const;

        void addNewInstance(size_t instanceId,
                            vkw::UniformBuffer<InstanceData> const &instanceBuffer);

        void eraseInstance(size_t instanceId);

        ~MMesh() override = default;
    };

    struct MNode {
        size_t index;
        std::weak_ptr<MNode> parent;
        std::vector<std::shared_ptr<MNode>> children;
        std::unique_ptr<MMesh> mesh;

        std::string name;



        // this fields are used to store initial data of a Node
        // and not used by actual instances

        MMesh::InstanceData initialData{};

        // And this array stores actual instance data

        // TODO: uniform buffer must not be here, bit rather near mesh

        std::map<size_t, std::pair<MMesh::InstanceData, vkw::UniformBuffer<MMesh::InstanceData>>>
                instanceBuffers;
    };

    class GLTFModelInstance;

    class GLTFModel {
        MaterialPool materials;
        std::optional<PipelinePool> m_pipelinePool{};
        ShaderPool m_shaderPool;
        std::vector<MaterialInfo> m_material_infos;
        std::vector<vkw::ColorImage2D> textures;
        std::vector<std::shared_ptr<MNode>> rootNodes;
        std::vector<std::shared_ptr<MNode>> linearNodes;
        std::reference_wrapper<vkw::Device> renderer_;
        vkw::Sampler sampler;
        std::stack<size_t> freeIDs;
        size_t instanceCount = 0;

        vkw::ColorImage2D &getTexture(int index) {
            return textures.at(index);
        }

        void loadImages(tinygltf::Model &gltfModel);

        void loadMaterials(tinygltf::Model &gltfModel);

        void loadNode(tinygltf::Model &gltfModel, std::shared_ptr<MNode> parent,
                      const tinygltf::Node &node, uint32_t nodeIndex);

        void destroyInstance(size_t id);

        void setRootMatrix(glm::mat4 transform, size_t id);

        void drawInstance(vkw::CommandBuffer &recorder, vkw::DescriptorSet const &globalSet, size_t id);

    public:

        GLTFModel(vkw::Device &renderer, ShaderLoader &loader, std::filesystem::path const &path);

        GLTFModelInstance createNewInstance();

        void setPipelinePool(vkw::RenderPass &pass, uint32_t subpass, vkw::DescriptorSetLayout const &globalLayout);

        friend class GLTFModelInstance;
    };

    class GLTFModelInstance {
        GLTFModel *model_;
        std::optional<size_t> id_;

        GLTFModelInstance(GLTFModel *model, size_t id) : model_(model), id_(id) {}

    public:
        GLTFModelInstance(GLTFModelInstance const &another) = delete;

        GLTFModelInstance(GLTFModelInstance &&another) noexcept {
            model_ = another.model_;
            id_ = another.id_;
            another.id_.reset();
        };

        GLTFModelInstance const &operator=(GLTFModelInstance const &another) = delete;

        GLTFModelInstance &operator=(GLTFModelInstance &&another) noexcept {
            model_ = another.model_;
            id_ = another.id_;
            another.id_.reset();
            return *this;
        }

        void setRotation(float x, float y, float z);

        void draw(vkw::CommandBuffer &recorder, vkw::DescriptorSet const &globalSet) {
            model_->drawInstance(recorder, globalSet, id_.value());
        };

        ~GLTFModelInstance();

        friend class GLTFModel;
    };

} // namespace Examples
#endif // RENDERAPITEST_MODEL_H
