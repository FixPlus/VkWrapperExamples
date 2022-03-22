#ifndef TESTAPP_WATERSURFACE_H
#define TESTAPP_WATERSURFACE_H

#include <glm/glm.hpp>
#include <vkw/Device.hpp>
#include <vkw/DescriptorSet.hpp>
#include <vkw/DescriptorPool.hpp>
#include <common/Camera.h>
#include <vkw/Pipeline.hpp>
#include "common/Utils.h"
#include "common/AssetImport.h"
#include "GlobalLayout.h"
#include "vkw/Sampler.hpp"

class WaterSurface {
public:
    static constexpr uint32_t TILE_DIM = 128;
    static constexpr float TILE_SIZE = 10.0f;
    struct PrimitiveAttrs
            : public vkw::AttributeBase<vkw::VertexAttributeType::VEC3F> {
        glm::vec3 pos;
    };

    struct UBO {
        glm::vec4 waves[4];
        glm::vec4 deepWaterColor = glm::vec4(0.0f, 0.01f, 0.3f, 1.0f);
        float time = 0.0f;
    } ubo;

    int cascades = 7;


    WaterSurface(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                 vkw::DescriptorSetLayout const &globalLayout, TestApp::ShaderLoader &loader);

    void update(float deltaTime) {
        ubo.time += deltaTime;
        *m_ubo_mapped = ubo;
        m_ubo.flush();
    }

    int totalTiles() const {
        return m_totalTiles;
    }

    void draw(vkw::CommandBuffer &buffer, GlobalLayout const &globalLayout);

private:
    vkw::GraphicsPipeline
    m_compile_pipeline(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass, TestApp::ShaderLoader &loader);


    enum class ConnectSide {
        NORTH = 0,
        EAST = 1,
        SOUTH = 2,
        WEST = 3,
        NO_CONNECT = 4
    };

    static void m_addConnectingEdge(ConnectSide side, std::vector<uint32_t> &indices);

    vkw::IndexBuffer<VK_INDEX_TYPE_UINT32> const &m_full_tile(ConnectSide side);

    static vkw::Sampler m_create_sampler(vkw::Device &device);


    vkw::VertexBuffer<PrimitiveAttrs> m_buffer;
    std::map<int, vkw::IndexBuffer<VK_INDEX_TYPE_UINT32>> m_full_tiles;

    vkw::UniformBuffer<UBO> m_ubo;
    UBO *m_ubo_mapped;
    vkw::DescriptorSetLayout m_descriptor_layout;
    vkw::PipelineLayout m_pipeline_layout;
    vkw::VertexShader m_vertex_shader;
    vkw::FragmentShader m_fragment_shader;
    vkw::GraphicsPipeline m_pipeline;
    vkw::DescriptorPool m_pool;
    vkw::DescriptorSet m_set;
    vkw::Sampler m_sampler;
    std::reference_wrapper<vkw::Device> m_device;

    int m_totalTiles = 0;
};

#endif //TESTAPP_WATERSURFACE_H
