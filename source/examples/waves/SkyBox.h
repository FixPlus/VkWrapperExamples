#ifndef TESTAPP_SKYBOX_H
#define TESTAPP_SKYBOX_H


#include "RenderEngine/AssetImport/AssetImport.h"
#include "vkw/Pipeline.hpp"
#include "RenderEngine/RecordingState.h"
#include "RenderEngine/Pipelines/Compute.h"
#include "common/Camera.h"
#include "common/GUI.h"
#include "common/Utils.h"

class SkyBox {
public:

    SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass, RenderEngine::ShaderLoaderInterface& shaderLoader);

    struct UBO{
        glm::vec4 viewportYAxis;
        glm::vec4 viewportXAxis;
        glm::vec4 viewportDirectionAxis;
        glm::vec4 cameraPos = glm::vec4(0.0f);
        glm::vec4 params;
    } ubo;

    struct Sun{
        glm::vec4 color = glm::vec4(1.0f);
        glm::vec4 params = glm::vec4(1.0f, 1.0f, 25.0f, 1.0f);
    } sun;

    struct Atmosphere{
        glm::vec4 K =  glm::vec4{0.15f, 0.404f, 0.532f, 0.02f}; // K.xyz - scattering constants in Rayleigh scatter model for rgb chanells accrodingly, k.w - scattering constant for Mie scattering
        glm::vec4 params = glm::vec4{1000000.0f, 100000.0f, 0.05f, -0.999f}; // x - planet radius, y - atmosphere radius, z - H0: atmosphere density factor, w - g: coef for Phase Function modeling Mie scattering
        int samples = 50;
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
        *m_atmo_mapped = atmosphere;
        m_atmo_buffer.flush();

        m_material.m_buffer.flush();
        m_material.m_material_buffer.flush();
    }

    RenderEngine::Geometry const& geom() const{
        return m_geometry;
    }

    vkw::UniformBuffer<Sun> const& sunBuffer() const{
        return m_material.m_material_buffer;
    }
    vkw::UniformBuffer<Atmosphere> const& atmoBuffer() const{
        return m_atmo_buffer;
    }

    vkw::ColorImage2DView const& outScatterTexture() const{
        return m_scatter_view.get();
    }

    glm::vec3 sunDirection() const{
        return glm::vec3{glm::sin(sun.params.x) * glm::sin(sun.params.y), glm::cos(sun.params.y), glm::cos(sun.params.x) * glm::sin(sun.params.y)};
    }

    void recomputeOutScatter() {
        m_out_scatter_texture.recompute(m_device);
    }
private:

    std::reference_wrapper<vkw::Device> m_device;

    RenderEngine::GeometryLayout m_geometry_layout;
    RenderEngine::ProjectionLayout m_projection_layout;
    RenderEngine::MaterialLayout m_material_layout;
    RenderEngine::LightingLayout m_lighting_layout;

    RenderEngine::Geometry m_geometry;
    RenderEngine::Projection m_projection;
    vkw::UniformBuffer<Atmosphere> m_atmo_buffer;
    Atmosphere* m_atmo_mapped;

    class OutScatterTexture: public vkw::ColorImage2D, public RenderEngine::ComputeLayout, public RenderEngine::Compute{
    public:
        OutScatterTexture(vkw::Device& device, RenderEngine::ShaderLoaderInterface& shaderLoader, vkw::UniformBuffer<Atmosphere> const& atmo, uint32_t psiRate, uint32_t heightRate);

        void recompute(vkw::Device& buffer);
    } m_out_scatter_texture;

    std::reference_wrapper<vkw::ColorImage2DView const> m_scatter_view;

    struct Material : RenderEngine::Material {
        vkw::UniformBuffer<UBO> m_buffer;
        vkw::UniformBuffer<Sun> m_material_buffer;

        UBO *m_mapped;
        Sun* m_material_mapped;


        Material(vkw::Device &device, RenderEngine::MaterialLayout &layout, vkw::UniformBuffer<Atmosphere> const& atmoBuf, vkw::ColorImage2DView const& outScatterTexture) : RenderEngine::Material(layout),
                                                                              m_buffer(device,
                                                                                       VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_mapped(m_buffer.map()),
                                                                              m_material_buffer(device,
                                                                                       VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_material_mapped(m_material_buffer.map()),
                                                                              m_sampler(TestApp::createDefaultSampler(device)){
            set().write(0, m_buffer);
            set().write(1, m_material_buffer);
            set().write(2, atmoBuf);
            set().write(3, outScatterTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
        }

        vkw::Sampler m_sampler;

    } m_material;



    RenderEngine::Lighting m_lighting;


};


class SkyBoxSettings: public TestApp::GUIWindow{
public:
    SkyBoxSettings(TestApp::GUIFrontEnd& gui, SkyBox& skybox, std::string const& title);
    bool needRecomputeOutScatter() const{
        return m_need_recompute_outscatter;
    }
protected:
    void onGui() override;

    std::reference_wrapper<SkyBox> m_skybox;
    bool m_need_recompute_outscatter = false;
};

#endif //TESTAPP_SKYBOX_H
