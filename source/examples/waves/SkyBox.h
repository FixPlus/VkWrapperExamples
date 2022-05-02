#ifndef TESTAPP_SKYBOX_H
#define TESTAPP_SKYBOX_H


#include "RenderEngine/AssetImport/AssetImport.h"
#include "vkw/Pipeline.hpp"
#include "RenderEngine/RecordingState.h"
#include "common/Camera.h"

class SkyBox {
public:

    SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass);

    struct UBO{
        glm::vec4 viewportYAxis;
        glm::vec4 viewportXAxis;
        glm::vec4 viewportDirectionAxis;
        glm::vec4 params;
    } ubo;

    struct MaterialUBO{
        glm::vec4 skyColor1 = glm::vec4(1.0f);
        glm::vec4 skyColor2 = glm::vec4(1.0f);
    } material;

    void draw(RenderEngine::GraphicsRecordingState &buffer);

    void update(TestApp::CameraPerspective const& camera) {

        ubo.params.x = camera.fov();
        ubo.params.y = camera.ratio();
        auto viewDirNormalized = glm::normalize(camera.viewDirection());
        ubo.viewportDirectionAxis = glm::vec4(viewDirNormalized, 0.0f);
        auto xDirNormalized = glm::normalize(glm::cross(viewDirNormalized, glm::vec3(0.0f, 1.0f, 0.0f)));
        ubo.viewportXAxis = glm::vec4(xDirNormalized, 0.0f);
        ubo.viewportYAxis = glm::vec4(glm::cross(xDirNormalized, viewDirNormalized), 0.0f);

        *m_material.m_mapped = ubo;
        *m_material.m_material_mapped = material;

        m_material.m_buffer.flush();
    }

    RenderEngine::Geometry const& geom() const{
        return m_geometry;
    }

    vkw::UniformBuffer<MaterialUBO> const& materialBuffer() const{
        return m_material.m_material_buffer;
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
        vkw::UniformBuffer<UBO> m_buffer;
        vkw::UniformBuffer<MaterialUBO> m_material_buffer;
        UBO *m_mapped;
        MaterialUBO* m_material_mapped;
        Material(vkw::Device &device, RenderEngine::MaterialLayout &layout) : RenderEngine::Material(layout),
                                                                              m_buffer(device,
                                                                                       VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_mapped(m_buffer.map()),
                                                                              m_material_buffer(device,
                                                                                       VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_material_mapped(m_material_buffer.map()){
            set().write(0, m_buffer);
            set().write(1, m_material_buffer);
        }
    } m_material;

    RenderEngine::Lighting m_lighting;

};

#endif //TESTAPP_SKYBOX_H
