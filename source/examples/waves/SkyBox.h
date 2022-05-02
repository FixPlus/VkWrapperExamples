#ifndef TESTAPP_SKYBOX_H
#define TESTAPP_SKYBOX_H


#include "RenderEngine/AssetImport/AssetImport.h"
#include "vkw/Pipeline.hpp"
#include "RenderEngine/RecordingState.h"
#include "common/Camera.h"
#include "common/GUI.h"

class SkyBox {
public:

    SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass);

    struct UBO{
        glm::vec4 viewportYAxis;
        glm::vec4 viewportXAxis;
        glm::vec4 viewportDirectionAxis;
        glm::vec4 cameraPos = glm::vec4(0.0f);
        glm::vec4 params;
    } ubo;

    struct Sun{
        glm::vec4 color = glm::vec4(1.0f);
        glm::vec4 params = glm::vec4(1.0f);
    } sun;

    struct Atmosphere{
        glm::vec4 K =  glm::vec4{0.1f, 0.2f, 0.8f, 0.5f}; // K.xyz - scattering constants in Rayleigh scatter model for rgb chanells accrodingly, k.w - scattering constant for Mie scattering
        glm::vec4 params = glm::vec4{1000000.0f, 10000.0f, 0.05f, -0.999f}; // x - planet radius, y - atmosphere radius, z - H0: atmosphere density factor, w - g: coef for Phase Function modeling Mie scattering
        int samples = 20;
    } atmosphere;

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
        *m_material.m_material_mapped = sun;
        *m_material.m_atmo_mapped = atmosphere;

        m_material.m_buffer.flush();
    }

    RenderEngine::Geometry const& geom() const{
        return m_geometry;
    }

    vkw::UniformBuffer<Sun> const& sunBuffer() const{
        return m_material.m_material_buffer;
    }

    glm::vec3 sunDirection() const{
        return glm::vec3{glm::sin(sun.params.x) * glm::sin(sun.params.y), glm::cos(sun.params.y), glm::cos(sun.params.x) * glm::sin(sun.params.y)};
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
        vkw::UniformBuffer<Sun> m_material_buffer;
        vkw::UniformBuffer<Atmosphere> m_atmo_buffer;
        UBO *m_mapped;
        Sun* m_material_mapped;
        Atmosphere* m_atmo_mapped;

        Material(vkw::Device &device, RenderEngine::MaterialLayout &layout) : RenderEngine::Material(layout),
                                                                              m_buffer(device,
                                                                                       VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_mapped(m_buffer.map()),
                                                                              m_material_buffer(device,
                                                                                       VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_material_mapped(m_material_buffer.map()),
                                                                              m_atmo_buffer(device,
                                                                                                VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_atmo_mapped(m_atmo_buffer.map()){
            set().write(0, m_buffer);
            set().write(1, m_material_buffer);
            set().write(2, m_atmo_buffer);
        }
    } m_material;

    RenderEngine::Lighting m_lighting;

};


class SkyBoxSettings: public TestApp::GUIWindow{
public:
    SkyBoxSettings(TestApp::GUIFrontEnd& gui, SkyBox& skybox, std::string const& title);
protected:
    void onGui() override;

    std::reference_wrapper<SkyBox> m_skybox;

};

#endif //TESTAPP_SKYBOX_H
