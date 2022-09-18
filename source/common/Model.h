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
#include <RenderEngine/AssetImport/AssetImport.h>
#include <Pipeline.hpp>
#include <DescriptorSet.hpp>
#include <DescriptorPool.hpp>
#include <Sampler.hpp>
#include <iostream>
#include <RenderEngine/Pipelines/PipelinePool.h>

namespace TestApp {

    enum class AttributeType {
        POSITION = 0, // 4f
        NORMAL = 1,   // 3f
        TANGENT = 2,  // 3f
        UV = 3,       // 2f
        COLOR = 4,    // 4f
        JOINT = 5,    // 4f
        WEIGHT = 6,   // 4f
        LAST = 7
    };

    using Texture2D = vkw::Image<vkw::COLOR, vkw::I2D>;
    using Texture2DView = vkw::ImageView<vkw::COLOR, vkw::V2D>;

    struct ModelAttributes : public vkw::AttributeBase<vkw::VertexAttributeType::VEC4F, vkw::VertexAttributeType::VEC3F, vkw::VertexAttributeType::VEC3F,
            vkw::VertexAttributeType::VEC2F, vkw::VertexAttributeType::VEC4F, vkw::VertexAttributeType::VEC4F, vkw::VertexAttributeType::VEC4F> {
        glm::vec4 pos;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec2 uv;
        glm::vec4 color;
        glm::vec4 joint;
        glm::vec4 weight;
    };

    class ModelMaterial;

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

        ModelMaterial const *material = nullptr;

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
        MeshBase(vkw::Device &device, tinygltf::Model const &model, tinygltf::Mesh const &mesh,
                 std::vector<ModelMaterial> const &materials, MeshCreateFlags flags = 0);

        void bindBuffers(RenderEngine::GraphicsRecordingState &recorder) const;

        /** bindBuffers must be called before executing this method */
        void drawPrimitive(RenderEngine::GraphicsRecordingState &recorder, int index) const;

        /** bindBuffers must be called before executing this method */
        void drawPrimitiveWithoutMaterial(RenderEngine::GraphicsRecordingState &recorder, int index) const;

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

    class DefaultTexturePool;

    struct MaterialInfo {
        Texture2D *colorMap = nullptr;
        Texture2D *normalMap = nullptr;
        Texture2D *metallicRoughnessMap = nullptr;
        vkw::Sampler const *sampler;

        auto operator<=>(MaterialInfo const &another) const = default;
    };

    class ModelMaterialLayout : public RenderEngine::MaterialLayout {
    public:
        explicit ModelMaterialLayout(vkw::Device &device);

    private:
    };

    class ModelMaterial : public RenderEngine::Material {
    public:
        ModelMaterial(vkw::Device &device, DefaultTexturePool& pool, ModelMaterialLayout &layout, MaterialInfo info);

        MaterialInfo const &info() const {
            return m_info;
        }

    private:
        Texture2DView m_colorMapView;
        Texture2DView m_normalMapView;
        Texture2DView m_mrMapView;

        MaterialInfo m_info;
    };

    class ModelGeometryLayout : public RenderEngine::GeometryLayout {
    public:
        explicit ModelGeometryLayout(vkw::Device &device);

    };

    class ModelGeometry : public RenderEngine::Geometry {
    public:
        struct Data {
            glm::mat4 transform = glm::mat4(1.0f);
        } data;

        ModelGeometry(vkw::Device &device, ModelGeometryLayout &layout);

        void update();

    private:
        vkw::UniformBuffer<Data> m_ubo;
        Data *m_mapped;
    };

    class MMesh;

    class MMesh : public MeshBase {

        std::map<size_t, ModelGeometry> m_instances{};

    public:
        struct InstanceData {
            glm::mat4 transform = glm::mat4(1.0f);
        };

        MMesh(vkw::Device &device, tinygltf::Model const &model,
              tinygltf::Node const &node, const std::vector<ModelMaterial> &materials,
              uint32_t maxInstances);

        void draw(RenderEngine::GraphicsRecordingState &recorder, size_t instanceId) const;

        void drawGeometryOnly(RenderEngine::GraphicsRecordingState &recorder, size_t instanceId) const;

        ModelGeometry const &instance(size_t id) const;

        ModelGeometry &instance(size_t id);

        ModelGeometry &addNewInstance(size_t instanceId, ModelGeometryLayout &layout, vkw::Device &device);

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

        ModelGeometry::Data initialData{};

        std::map<uint32_t, ModelGeometry::Data> instanceBuffers;
    };

    class GLTFModelInstance;

    class DefaultTexturePool{
    public:

        explicit DefaultTexturePool(vkw::Device& device, uint32_t textureDim = 256);

        Texture2D& colorMap(){
            return m_colorMap;
        }

        Texture2D& normalMap(){
            return m_normalMap;
        }

        Texture2D& metallicRoughnessMap(){
            return m_metallicRoughnessMap;
        }


    private:
        Texture2D m_colorMap;
        Texture2D m_normalMap;
        Texture2D m_metallicRoughnessMap;
    };

    class GLTFModel {
        ModelMaterialLayout materialLayout;
        ModelGeometryLayout geometryLayout;
        std::vector<ModelMaterial> materials;

        std::vector<Texture2D> textures;
        std::vector<std::shared_ptr<MNode>> rootNodes;
        std::vector<std::shared_ptr<MNode>> linearNodes;
        std::reference_wrapper<vkw::Device> renderer_;
        std::reference_wrapper<DefaultTexturePool> m_defaultTexturePool;
        vkw::Sampler sampler;
        std::stack<size_t> freeIDs;
        size_t instanceCount = 0;

        Texture2D &getTexture(int index) {
            return textures.at(index);
        }

        void loadImages(tinygltf::Model &gltfModel);

        void loadMaterials(tinygltf::Model &gltfModel);

        void loadNode(tinygltf::Model &gltfModel, std::shared_ptr<MNode> parent,
                      const tinygltf::Node &node, uint32_t nodeIndex);

        void destroyInstance(size_t id);

        void setRootMatrix(glm::mat4 transform, size_t id);

        void drawInstance(RenderEngine::GraphicsRecordingState &recorder, size_t id);

        void drawInstanceGeometryOnly(RenderEngine::GraphicsRecordingState &recorder, size_t id);

    public:

        GLTFModel(vkw::Device &renderer, DefaultTexturePool& pool, std::filesystem::path const &path);

        GLTFModelInstance createNewInstance();

        friend class GLTFModelInstance;
    };

    class GLTFModelInstance {
        GLTFModel *model_;
        std::optional<size_t> id_;


        GLTFModelInstance(GLTFModel *model, size_t id) : model_(model), id_(id) {}

    public:
        glm::vec3 rotation = glm::vec3{0.0f};
        glm::vec3 scale = glm::vec3{1.0f};
        glm::vec3 translation = glm::vec3{0.0f};

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

        void update();

        void draw(RenderEngine::GraphicsRecordingState &recorder) {
            model_->drawInstance(recorder, id_.value());
        };

        void drawGeometryOnly(RenderEngine::GraphicsRecordingState &recorder) {
            model_->drawInstanceGeometryOnly(recorder, id_.value());
        };

        ~GLTFModelInstance();

        friend class GLTFModel;
    };

} // namespace Examples
#endif // RENDERAPITEST_MODEL_H
