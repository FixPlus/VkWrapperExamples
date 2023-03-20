#ifndef TESTAPP_PLANET_HPP
#define TESTAPP_PLANET_HPP

#include "Atmosphere.hpp"
#include "BumpMap.hpp"
#include "Camera.h"
#include "GUI.h"
#include "SphereMesh.hpp"
#include "SunLight.hpp"

#include <vkw/RenderPass.hpp>

namespace TestApp {

class PlanetMesh : public SphereMesh {
public:
  PlanetMesh(vkw::Device &device, unsigned subdivisions)
      : SphereMesh(device, subdivisions, false) {}

  void bind(RenderEngine::GraphicsRecordingState &recorder) const {
    recorder.commands().bindVertexBuffer(vertexBuffer(), 0, 0);
    recorder.commands().bindIndexBuffer(indexBuffer(), 0);
  }

  void draw(RenderEngine::GraphicsRecordingState &recorder) const {
    recorder.commands().drawIndexed(indexCount(), 1);
  }
};

class PlanetPool {
public:
  PlanetPool(vkw::Device &device,
             RenderEngine::ShaderLoaderInterface &shaderLoader,
             SunLight &sunlight, Camera const &camera, vkw::RenderPass &pass,
             unsigned subpass, unsigned maxPlanets = 10);

  struct MeshGeometryLayout : public RenderEngine::GeometryLayout {
    MeshGeometryLayout(vkw::Device &device, unsigned maxSets);
  };

  struct EmissiveSurfaceProjectionLayout
      : public RenderEngine::ProjectionLayout {
    EmissiveSurfaceProjectionLayout(vkw::Device &device, unsigned maxSets);
  };

  struct TransparentSurfaceProjectionLayout
      : public RenderEngine::ProjectionLayout {
    TransparentSurfaceProjectionLayout(vkw::Device &device, unsigned maxSets);
  };

  struct SkyDomeMaterialLayout : public RenderEngine::MaterialLayout {
    SkyDomeMaterialLayout(vkw::Device &device, unsigned maxSets);
  };

  struct PlanetSurfaceMaterialLayout : public RenderEngine::MaterialLayout {
    PlanetSurfaceMaterialLayout(vkw::Device &device, unsigned maxSets);
  };

  struct EmissiveLightingLayout : public RenderEngine::LightingLayout {
    EmissiveLightingLayout(vkw::Device &device, vkw::RenderPass &pass,
                           unsigned subpass, unsigned maxSets);
  };

  struct TransparentLightingLayout : public RenderEngine::LightingLayout {
    TransparentLightingLayout(vkw::Device &device, vkw::RenderPass &pass,
                              unsigned subpass, unsigned maxSets);
  };

  void update();

  void
  setSkyDomeMaterial(RenderEngine::GraphicsRecordingState &recorder) const {
    recorder.setMaterial(m_skyDomeMaterial);
  }

  void bindMesh(RenderEngine::GraphicsRecordingState &recorder) const {
    m_baseMesh.bind(recorder);
  }

  void drawMesh(RenderEngine::GraphicsRecordingState &recorder) const {
    m_baseMesh.draw(recorder);
  }

  void
  setEmissiveLighting(RenderEngine::GraphicsRecordingState &recorder) const {
    recorder.setLighting(m_emissiveLighting);
  }

  void
  setTransparentLighting(RenderEngine::GraphicsRecordingState &recorder) const {
    recorder.setLighting(m_transparentLighting);
  }

  auto &cameraBuffer() const { return m_cameraUbo; }

  auto &sunlight() const { return m_sunlight.get(); }

  auto &meshGeometryLayout() { return m_meshGeometryLayout; }

  auto &emissiveProjLayout() { return m_emissiveProjLayout; }

  auto &transparentProjLayout() { return m_transparentProjLayout; }

  auto &planetSurfaceMaterialLayout() { return m_planetSurfaceMaterialLayout; }

  auto &shaderLoader() { return m_shaderLoader.get(); }
  auto &device() { return m_device.get(); }

  SphereMesh &mesh() { return m_baseMesh; }

private:
  MeshGeometryLayout m_meshGeometryLayout;
  EmissiveSurfaceProjectionLayout m_emissiveProjLayout;
  TransparentSurfaceProjectionLayout m_transparentProjLayout;
  SkyDomeMaterialLayout m_skyDomeMaterialLayout;
  RenderEngine::Material m_skyDomeMaterial;
  PlanetSurfaceMaterialLayout m_planetSurfaceMaterialLayout;
  EmissiveLightingLayout m_emissiveLightingLayout;
  TransparentLightingLayout m_transparentLightingLayout;
  RenderEngine::Lighting m_emissiveLighting;
  RenderEngine::Lighting m_transparentLighting;
  PlanetMesh m_baseMesh;
  vkw::UniformBuffer<std::pair<glm::mat4, glm::mat4>> m_cameraUbo;
  std::reference_wrapper<const Camera> m_camera;
  std::reference_wrapper<const SunLight> m_sunlight;
  std::reference_wrapper<RenderEngine::ShaderLoaderInterface> m_shaderLoader;
  vkw::StrongReference<vkw::Device> m_device;
};

class PlanetTexture : public RenderEngine::Material {
public:
  PlanetTexture(vkw::Device &device, PlanetPool &pool,
                vkw::Image<vkw::COLOR, vkw::I2D> const &colorMap,
                BumpMap const &bumpMap);

  struct LandscapeProps {
    float height = 1.0f;
    float planetRadius = 1.0f;
    float pad[2];
    int samples = 10;
  } landscapeProps;

  void update();

private:
  vkw::Sampler m_sampler;
  vkw::ImageView<vkw::COLOR, vkw::V2D> m_colorMap;
  vkw::ImageView<vkw::COLOR, vkw::V2D> m_bumpMap;
  vkw::UniformBuffer<LandscapeProps> m_landscapeUbo;
  vkw::StrongReference<vkw::Device> m_device;
};

class Planet {
public:
  struct Properties {
    glm::vec3 position = glm::vec3{0.0f};
    glm::vec2 rotation = glm::vec2{0.0f};
    float planetRadius = 1000.0f;
    float atmosphereRadius = 50.0f;
  } properties;

  auto &atmosphere() { return m_atmosphere; }

  auto &surface() { return m_surfaceMaterial.get(); }

  Planet(PlanetPool &planetPool, PlanetTexture &texture);

  void drawSkyDome(RenderEngine::GraphicsRecordingState &recorder);

  void drawSurface(RenderEngine::GraphicsRecordingState &recorder);

  void update();

private:
  Atmosphere m_atmosphere;
  class MeshGeometry : public RenderEngine::Geometry {
  public:
    MeshGeometry(vkw::Device &device, PlanetPool::MeshGeometryLayout &layout,
                 Properties const &props);

    void update(Properties const &props);

  private:
    vkw::UniformBuffer<glm::mat4> m_propUbo;
    vkw::StrongReference<vkw::Device> m_device;
  } m_meshGeometry;

  class SkyDomeProjection : public RenderEngine::Projection {
  public:
    SkyDomeProjection(
        vkw::Device &device,
        PlanetPool::TransparentSurfaceProjectionLayout &layout,
        Atmosphere const &atmosphere, SunLight const &sunLight,
        vkw::UniformBuffer<std::pair<glm::mat4, glm::mat4>> const &cameraBuf);
  } m_skyDomeProj;

  class SurfaceProjection : public RenderEngine::Projection {
  public:
    SurfaceProjection(
        vkw::Device &device,
        PlanetPool::EmissiveSurfaceProjectionLayout &layout,
        Atmosphere const &atmosphere, SunLight const &sunLight,
        vkw::UniformBuffer<std::pair<glm::mat4, glm::mat4>> const &cameraBuf);
  } m_surfaceProj;

  std::reference_wrapper<PlanetTexture> m_surfaceMaterial;

  std::reference_wrapper<PlanetPool> m_planetPool;
};

class PlanetProperties : public AtmosphereProperties {
public:
  PlanetProperties(GUIFrontEnd &gui, Planet &planet,
                   std::string_view title = "Planet properties");

protected:
  void onGui() override;

private:
  glm::vec2 m_rotDeg;
  std::reference_wrapper<Planet> m_planet;
};

} // namespace TestApp
#endif // TESTAPP_PLANET_HPP
