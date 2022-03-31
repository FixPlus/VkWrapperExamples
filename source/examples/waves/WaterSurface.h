#ifndef TESTAPP_WATERSURFACE_H
#define TESTAPP_WATERSURFACE_H

#include <glm/glm.hpp>
#include <vkw/Device.hpp>
#include <vkw/DescriptorSet.hpp>
#include <vkw/DescriptorPool.hpp>
#include <common/Camera.h>
#include <vkw/Pipeline.hpp>
#include "common/Utils.h"
#include "RenderEngine/AssetImport/AssetImport.h"
#include "GlobalLayout.h"
#include "vkw/Sampler.hpp"
#include "RenderEngine/RecordingState.h"

class WaterSurface : public RenderEngine::GeometryLayout {
public:
    struct PrimitiveAttrs
            : public vkw::AttributeBase<vkw::VertexAttributeType::VEC3F> {
        glm::vec3 pos;
    };

    struct UBO {
        glm::vec4 waves[4];
        float time = 0.0f;
    } ubo;

    static constexpr uint32_t TILE_DIM = 64;
    static constexpr float TILE_SIZE = 20.0f;

    int cascades = 7;
    bool wireframe = false;
    float tileScale = 1.0f;
    float elevationScale = 0.1f;

    void update(float deltaTime) {
        ubo.time += deltaTime;
        *m_geometry.m_ubo_mapped = ubo;
        m_geometry.m_ubo.flush();
    }

    WaterSurface(vkw::Device &device);

    void draw(RenderEngine::GraphicsRecordingState &buffer, GlobalLayout const &globalLayout);

    int totalTiles() const {
        return m_totalTiles;
    }

private:
    enum class ConnectSide {
        NORTH = 0,
        EAST = 1,
        SOUTH = 2,
        WEST = 3,
        NO_CONNECT = 4
    };

    static void m_addConnectingEdge(ConnectSide side, std::vector<uint32_t> &indices);

    vkw::IndexBuffer<VK_INDEX_TYPE_UINT32> const &m_full_tile(ConnectSide side);

    struct Geometry : public RenderEngine::Geometry {
        vkw::UniformBuffer<UBO> m_ubo;
        UBO *m_ubo_mapped;

        Geometry(vkw::Device &device, WaterSurface &surface);
    } m_geometry;

    int m_totalTiles = 0;
    vkw::VertexBuffer<PrimitiveAttrs> m_buffer;
    std::map<int, vkw::IndexBuffer<VK_INDEX_TYPE_UINT32>> m_full_tiles;
    std::reference_wrapper<vkw::Device> m_device;

};


class WaterMaterial : public RenderEngine::MaterialLayout {
public:
    WaterMaterial(vkw::Device &device, bool wireframe = false) : RenderEngine::MaterialLayout(device,
                                                                                              RenderEngine::MaterialLayout::CreateInfo{.substageDescription=RenderEngine::SubstageDescription{.shaderSubstageName="water", .setBindings={
                                                                                                      vkw::DescriptorSetLayoutBinding{
                                                                                                              0,
                                                                                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}}, .rasterizationState={
                                                                                                      VK_FALSE,
                                                                                                      VK_FALSE,
                                                                                                      wireframe
                                                                                                      ? VK_POLYGON_MODE_LINE
                                                                                                      : VK_POLYGON_MODE_FILL}, .depthTestState=vkw::DepthTestStateCreateInfo{
                                                                                                      VK_COMPARE_OP_LESS,
                                                                                                      true}, .maxMaterials=1}),
                                                                 m_material(device, *this) {

    };

    struct WaterDescription {
        glm::vec4 deepWaterColor = glm::vec4(0.0f, 0.01f, 0.3f, 1.0f);
        float metallic = 0.2f;
        float roughness = 0.2f;
    } description;

    RenderEngine::Material const &get() const {
        return m_material;
    }

    void update() {
        *m_material.m_mapped = description;
        m_material.m_buffer.map();
    }

private:
    struct Material : public RenderEngine::Material {
        vkw::UniformBuffer<WaterDescription> m_buffer;
        WaterDescription *m_mapped;

        Material(vkw::Device &device, WaterMaterial &waterMaterial);
    } m_material;

};

#endif //TESTAPP_WATERSURFACE_H
