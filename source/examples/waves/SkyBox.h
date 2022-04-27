#ifndef TESTAPP_SKYBOX_H
#define TESTAPP_SKYBOX_H


#include "GlobalLayout.h"
#include "RenderEngine/AssetImport/AssetImport.h"
#include "vkw/Pipeline.hpp"
#include "RenderEngine/RecordingState.h"


class SkyBox {
public:

    SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass);

    glm::vec4 lightColor;

    void draw(RenderEngine::GraphicsRecordingState &buffer);

    void update() {
        *m_material.m_mapped = lightColor;
        m_material.m_buffer.flush();
    }

    RenderEngine::Geometry const& geom() const{
        return m_geometry;
    }
private:

    std::reference_wrapper<vkw::Device> m_device;

    RenderEngine::GeometryLayout m_geometry_layout;
    RenderEngine::ProjectionLayout m_projection_layout;
    RenderEngine::MaterialLayout m_material_layout;
    RenderEngine::LightingLayout m_lighting_layout;

    RenderEngine::Geometry m_geometry;
    RenderEngine::Projection m_projection;

    struct Material : RenderEngine::Material {
        vkw::UniformBuffer<glm::vec4> m_buffer;
        glm::vec4 *m_mapped;

        Material(vkw::Device &device, RenderEngine::MaterialLayout &layout) : RenderEngine::Material(layout),
                                                                              m_buffer(device,
                                                                                       VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_mapped(m_buffer.map()) {
            set().write(0, m_buffer);
        }
    } m_material;

    RenderEngine::Lighting m_lighting;

};

#endif //TESTAPP_SKYBOX_H
