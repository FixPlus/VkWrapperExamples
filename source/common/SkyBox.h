#ifndef TESTAPP_SKYBOX_H
#define TESTAPP_SKYBOX_H

#include "RenderEngine/AssetImport/AssetImport.h"
#include "RenderEngine/Pipelines/Compute.h"
#include "RenderEngine/RecordingState.h"
#include "common/Camera.h"
#include "common/GUI.h"
#include "common/Utils.h"
#include "vkw/Pipeline.hpp"

class SkyBox : public vkw::ReferenceGuard {
public:
  SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass,
         RenderEngine::ShaderLoaderInterface &shaderLoader);

  struct UBO {
    glm::mat4 invProjView;
    glm::vec4 cameraPos;
    float near_plane;
    float far_plane;
  } ubo;

  struct Sun {
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec4 params = glm::vec4(1.0f, 1.0f, 25.0f, 1.0f);
  } sun;

  struct Atmosphere {
    glm::vec4 K =
        glm::vec4{0.02f, 0.085f, 0.128f,
                  0.04f}; // K.xyz - scattering constants in Rayleigh scatter
                          // model for rgb chanells accrodingly, k.w -
                          // scattering constant for Mie scattering
    glm::vec4 params =
        glm::vec4{1000000.0f, 100000.0f, 0.25f,
                  -0.9999f}; // x - planet radius, y - atmosphere radius, z -
                             // H0: atmosphere density factor, w - g: coef for
                             // Phase Function modeling Mie scattering
    int samples = 10;
  } atmosphere;

  void draw(RenderEngine::GraphicsRecordingState &buffer);

  void update(TestApp::CameraPerspective const &camera) {

    ubo.invProjView = glm::inverse(camera.projection() * camera.cameraSpace());
    ubo.far_plane = camera.farPlane();
    ubo.near_plane = camera.nearPlane();
    ubo.cameraPos = glm::vec4(camera.position(), 1.0f);

    *m_material.m_mapped = ubo;
    *m_material.m_material_mapped = sun;
    *m_atmo_mapped = atmosphere;
    m_atmo_buffer.flush();

    m_material.m_buffer.flush();
    m_material.m_material_buffer.flush();
  }

  RenderEngine::Geometry const &geom() const { return m_geometry; }

  vkw::UniformBuffer<Sun> const &sunBuffer() const {
    return m_material.m_material_buffer;
  }
  vkw::UniformBuffer<Atmosphere> const &atmoBuffer() const {
    return m_atmo_buffer;
  }

  vkw::ImageView<vkw::COLOR, vkw::V2D> const &outScatterTexture() const {
    return m_out_scatter_texture.m_view;
  }

  glm::vec3 sunDirection() const {
    return glm::vec3{glm::sin(sun.params.x) * glm::sin(sun.params.y),
                     glm::cos(sun.params.y),
                     glm::cos(sun.params.x) * glm::sin(sun.params.y)};
  }

  void recomputeOutScatter() { m_out_scatter_texture.recompute(m_device); }

private:
  vkw::StrongReference<vkw::Device> m_device;

  RenderEngine::GeometryLayout m_geometry_layout;
  RenderEngine::ProjectionLayout m_projection_layout;
  RenderEngine::MaterialLayout m_material_layout;
  RenderEngine::LightingLayout m_lighting_layout;

  RenderEngine::Geometry m_geometry;
  RenderEngine::Projection m_projection;
  vkw::UniformBuffer<Atmosphere> m_atmo_buffer;
  Atmosphere *m_atmo_mapped;

  class OutScatterTexture : public vkw::Image<vkw::COLOR, vkw::I2D>,
                            public RenderEngine::ComputeLayout,
                            public RenderEngine::Compute {
  public:
    vkw::ImageView<vkw::COLOR, vkw::V2D> m_view;

    OutScatterTexture(vkw::Device &device,
                      RenderEngine::ShaderLoaderInterface &shaderLoader,
                      vkw::UniformBuffer<Atmosphere> const &atmo,
                      uint32_t psiRate, uint32_t heightRate);

    void recompute(vkw::Device &buffer);
  } m_out_scatter_texture;

  struct Material : RenderEngine::Material {
    vkw::UniformBuffer<UBO> m_buffer;
    vkw::UniformBuffer<Sun> m_material_buffer;

    UBO *m_mapped;
    Sun *m_material_mapped;

    Material(vkw::Device &device, RenderEngine::MaterialLayout &layout,
             vkw::UniformBuffer<Atmosphere> const &atmoBuf,
             vkw::ImageView<vkw::COLOR, vkw::V2D> const &outScatterTexture)
        : RenderEngine::Material(layout),
          m_buffer(device,
                   VmaAllocationCreateInfo{
                       .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                       .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
          m_material_buffer(
              device,
              VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                      .requiredFlags =
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
          m_sampler(TestApp::createDefaultSampler(device)) {
      m_buffer.map();
      m_material_buffer.map();
      m_mapped = m_buffer.mapped().data();
      m_material_mapped = m_material_buffer.mapped().data();
      set().write(0, m_buffer);
      set().write(1, m_material_buffer);
      set().write(2, atmoBuf);
      set().write(3, outScatterTexture,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
    }

    vkw::Sampler m_sampler;

  } m_material;

  RenderEngine::Lighting m_lighting;
};

class SkyBoxSettings : public TestApp::GUIWindow {
public:
  SkyBoxSettings(TestApp::GUIFrontEnd &gui, SkyBox &skybox,
                 std::string const &title);
  bool needRecomputeOutScatter() const { return m_need_recompute_outscatter; }

protected:
  void onGui() override;

  vkw::StrongReference<SkyBox> m_skybox;
  bool m_need_recompute_outscatter = false;
};

#endif // TESTAPP_SKYBOX_H
