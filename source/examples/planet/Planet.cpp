#include "Planet.hpp"
#include "Utils.h"

namespace TestApp {

PlanetPool::PlanetPool(vkw::Device &device,
                       RenderEngine::ShaderLoaderInterface &shaderLoader,
                       SunLight &sunlight, const Camera &camera,
                       vkw::RenderPass &pass, unsigned int subpass,
                       unsigned maxPlanets)
    : m_meshGeometryLayout(device, maxPlanets),
      m_emissiveProjLayout(device, maxPlanets),
      m_transparentProjLayout(device, maxPlanets),
      m_planetSurfaceMaterialLayout(device, maxPlanets),
      m_emissiveLightingLayout(device, pass, subpass, 1),
      m_transparentLightingLayout(device, pass, subpass, 1),
      m_skyDomeMaterialLayout(device, 1), m_device(device),
      m_shaderLoader(shaderLoader), m_sunlight(sunlight), m_camera(camera),
      m_cameraUbo(device,
                  VmaAllocationCreateInfo{
                      .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                      .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
      m_baseMesh(device, 6), m_transparentLighting(m_transparentLightingLayout),
      m_emissiveLighting(m_emissiveLightingLayout),
      m_skyDomeMaterial(m_skyDomeMaterialLayout) {
  m_cameraUbo.map();
}

void PlanetPool::update() {
  m_cameraUbo.mapped().front().first = m_camera.get().cameraSpace();
  m_cameraUbo.mapped().front().second = m_camera.get().projection();
  m_cameraUbo.flush();
}

Planet::Planet(PlanetPool &planetPool, PlanetTexture &texture)
    : m_atmosphere(planetPool.device(), planetPool.shaderLoader()),
      m_meshGeometry(planetPool.device(), planetPool.meshGeometryLayout(),
                     properties),
      m_skyDomeProj(planetPool.device(), planetPool.transparentProjLayout(),
                    m_atmosphere, planetPool.sunlight(),
                    planetPool.cameraBuffer()),
      m_surfaceProj(planetPool.device(), planetPool.emissiveProjLayout(),
                    m_atmosphere, planetPool.sunlight(),
                    planetPool.cameraBuffer()),
      m_surfaceMaterial(texture), m_planetPool(planetPool) {
  update();
}

void Planet::update() {
  m_atmosphere.properties.planetRadius = properties.planetRadius;
  m_atmosphere.properties.atmosphereRadius = properties.atmosphereRadius;
  m_atmosphere.properties.planetPosition = glm::vec4{properties.position, 1.0f};
  m_surfaceMaterial.get().landscapeProps.planetRadius = properties.planetRadius;
  m_surfaceMaterial.get().update();
  m_atmosphere.update();
  m_meshGeometry.update(properties);
}

void Planet::drawSkyDome(RenderEngine::GraphicsRecordingState &recorder) {
  m_planetPool.get().setTransparentLighting(recorder);
  recorder.setGeometry(m_meshGeometry);
  recorder.setProjection(m_skyDomeProj);
  m_planetPool.get().setSkyDomeMaterial(recorder);
  recorder.bindPipeline();
  recorder.pushConstants(
      (properties.atmosphereRadius + properties.planetRadius) /
          properties.planetRadius,
      VK_SHADER_STAGE_VERTEX_BIT, 0);
  m_planetPool.get().drawMesh(recorder);
}

void Planet::drawSurface(RenderEngine::GraphicsRecordingState &recorder) {
  m_planetPool.get().setEmissiveLighting(recorder);
  recorder.setGeometry(m_meshGeometry);
  recorder.setProjection(m_surfaceProj);
  recorder.setMaterial(m_surfaceMaterial.get());
  recorder.bindPipeline();
  recorder.pushConstants(1.0f, VK_SHADER_STAGE_VERTEX_BIT, 0);
  m_planetPool.get().drawMesh(recorder);
}

Planet::MeshGeometry::MeshGeometry(vkw::Device &device,
                                   PlanetPool::MeshGeometryLayout &layout,
                                   const Planet::Properties &props)
    : RenderEngine::Geometry(layout),
      m_propUbo(device,
                VmaAllocationCreateInfo{
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                    .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                VK_BUFFER_USAGE_TRANSFER_DST_BIT),
      m_device(device) {
  set().write(0, m_propUbo);
  update(props);
}

void Planet::MeshGeometry::update(const Planet::Properties &props) {
  glm::mat4 transform = glm::mat4{1.0f};

  transform = glm::scale(transform, glm::vec3{props.planetRadius});
  transform =
      glm::rotate(transform, props.rotation.y, glm::vec3{1.0f, 0.0f, 0.0f});
  transform =
      glm::rotate(transform, props.rotation.x, glm::vec3{0.0f, 1.0f, 0.0f});

  transform = glm::translate(transform, props.position);

  loadUsingStaging(m_device.get(), m_propUbo, transform);
}

Planet::SkyDomeProjection::SkyDomeProjection(
    vkw::Device &device, PlanetPool::TransparentSurfaceProjectionLayout &layout,
    const Atmosphere &atmosphere, const SunLight &sunLight,
    const vkw::UniformBuffer<std::pair<glm::mat4, glm::mat4>> &cameraBuf)
    : RenderEngine::Projection(layout) {
  set().write(0, atmosphere.propertiesBuffer());
  set().write(1, sunLight.propertiesBuffer());
  set().write(2, cameraBuf);
  set().write(3, atmosphere.outScatterTexture(),
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              atmosphere.outScatterTextureSampler());
}

Planet::SurfaceProjection::SurfaceProjection(
    vkw::Device &device, PlanetPool::EmissiveSurfaceProjectionLayout &layout,
    const Atmosphere &atmosphere, const SunLight &sunLight,
    const vkw::UniformBuffer<std::pair<glm::mat4, glm::mat4>> &cameraBuf)
    : RenderEngine::Projection(layout) {
  set().write(0, atmosphere.propertiesBuffer());
  set().write(1, sunLight.propertiesBuffer());
  set().write(2, cameraBuf);
  set().write(3, atmosphere.outScatterTexture(),
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              atmosphere.outScatterTextureSampler());
}

PlanetTexture::PlanetTexture(vkw::Device &device, PlanetPool &pool,
                             const vkw::Image<vkw::COLOR, vkw::I2D> &colorMap,
                             BumpMap const &bumpMap)
    : RenderEngine::Material(pool.planetSurfaceMaterialLayout()),
      m_sampler(createDefaultSampler(device)),
      m_colorMap(device, colorMap, colorMap.format()),
      m_bumpMap(device, bumpMap, bumpMap.format()),
      m_landscapeUbo(device,
                     VmaAllocationCreateInfo{
                         .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                         .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT),
      m_device(device) {
  set().write(0, m_colorMap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              m_sampler);
  set().write(1, m_bumpMap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              m_sampler);
  set().write(2, m_landscapeUbo);
  update();
}

void PlanetTexture::update() {
  loadUsingStaging(m_device.get(), m_landscapeUbo, landscapeProps);
}

PlanetPool::MeshGeometryLayout::MeshGeometryLayout(vkw::Device &device,
                                                   unsigned int maxSets)
    : RenderEngine::GeometryLayout(
          device,
          RenderEngine::GeometryLayout::CreateInfo{
              std::make_unique<vkw::VertexInputStateCreateInfo<
                  vkw::per_vertex<SphereMesh::Vertex, 0>>>(),
              vkw::InputAssemblyStateCreateInfo{},
              RenderEngine::SubstageDescription{
                  "planet",
                  {vkw::DescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}},
                  {VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                       .offset = 0,
                                       .size = sizeof(float)}}},
              maxSets}) {}

PlanetPool::EmissiveSurfaceProjectionLayout::EmissiveSurfaceProjectionLayout(
    vkw::Device &device, unsigned int maxSets)
    : RenderEngine::ProjectionLayout(
          device,
          RenderEngine::SubstageDescription{
              "scattering_emissive",
              {vkw::DescriptorSetLayoutBinding{
                   0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
               vkw::DescriptorSetLayoutBinding{
                   1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
               vkw::DescriptorSetLayoutBinding{
                   2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
               vkw::DescriptorSetLayoutBinding{
                   3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}},
          maxSets) {}

PlanetPool::TransparentSurfaceProjectionLayout::
    TransparentSurfaceProjectionLayout(vkw::Device &device,
                                       unsigned int maxSets)
    : RenderEngine::ProjectionLayout(
          device,
          RenderEngine::SubstageDescription{
              "scattering_transparent",
              {vkw::DescriptorSetLayoutBinding{
                   0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
               vkw::DescriptorSetLayoutBinding{
                   1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
               vkw::DescriptorSetLayoutBinding{
                   2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
               vkw::DescriptorSetLayoutBinding{
                   3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}},
          maxSets) {}

PlanetPool::SkyDomeMaterialLayout::SkyDomeMaterialLayout(vkw::Device &device,
                                                         unsigned int maxSets)
    : RenderEngine::MaterialLayout(
          device, RenderEngine::MaterialLayout::CreateInfo{
                      RenderEngine::SubstageDescription{"skydome_surface"},
                      vkw::RasterizationStateCreateInfo{
                          false, false, VK_POLYGON_MODE_FILL,
                          VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE},
                      vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS, false},
                      maxSets}) {}

PlanetPool::PlanetSurfaceMaterialLayout::PlanetSurfaceMaterialLayout(
    vkw::Device &device, unsigned int maxSets)
    : RenderEngine::MaterialLayout(
          device,
          RenderEngine::MaterialLayout::CreateInfo{
              RenderEngine::SubstageDescription{
                  "planet_surface",
                  {vkw::DescriptorSetLayoutBinding{
                       0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                   vkw::DescriptorSetLayoutBinding{
                       1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                   vkw::DescriptorSetLayoutBinding{
                       2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}},
              vkw::RasterizationStateCreateInfo{
                  false, false, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                  VK_FRONT_FACE_COUNTER_CLOCKWISE},
              vkw::DepthTestStateCreateInfo{VK_COMPARE_OP_LESS, true},
              maxSets}) {}

PlanetPool::TransparentLightingLayout::TransparentLightingLayout(
    vkw::Device &device, vkw::RenderPass &pass, unsigned int subpass,
    unsigned int maxSets)
    : RenderEngine::LightingLayout(
          device,
          RenderEngine::LightingLayout::CreateInfo{
              RenderEngine::SubstageDescription{"scattering_transparent"}, pass,
              subpass},
          maxSets) {}

PlanetPool::EmissiveLightingLayout::EmissiveLightingLayout(
    vkw::Device &device, vkw::RenderPass &pass, unsigned int subpass,
    unsigned int maxSets)
    : RenderEngine::LightingLayout(
          device,
          RenderEngine::LightingLayout::CreateInfo{
              RenderEngine::SubstageDescription{"scattering_emissive"}, pass,
              subpass},
          maxSets) {}

PlanetProperties::PlanetProperties(GUIFrontEnd &gui, Planet &planet,
                                   std::string_view title)
    : AtmosphereProperties(gui, planet.atmosphere(), title), m_planet(planet) {
  disableRadiusTrack();
}

void PlanetProperties::onGui() {
  if (ImGui::CollapsingHeader("Atmosphere")) {
    AtmosphereProperties::onGui();
  }

  bool needUpdate = false;

  if (ImGui::CollapsingHeader("Surface")) {
    if (ImGui::SliderFloat("Terrain height",
                           &m_planet.get().surface().landscapeProps.height,
                           0.0f, 100.0f))
      needUpdate = true;
    if (ImGui::SliderInt("POM samples",
                         &m_planet.get().surface().landscapeProps.samples, 5,
                         50))
      needUpdate = true;
  }

  if (!ImGui::CollapsingHeader("General")) {
    if (needUpdate)
      m_planet.get().update();
    return;
  }

  if (ImGui::SliderFloat("radius", &m_planet.get().properties.planetRadius,
                         10.0f, 1000.0f))
    needUpdate = true;
  if (ImGui::SliderFloat("atmosphere radius",
                         &m_planet.get().properties.atmosphereRadius, 1.0f,
                         100.0f))
    needUpdate = true;
  if (ImGui::SliderFloat("rotate phi", &m_rotDeg.x, 0.0f, 360.0f)) {
    m_planet.get().properties.rotation.x = glm::radians(m_rotDeg.x);
    needUpdate = true;
  }
  if (ImGui::SliderFloat("rotate psi", &m_rotDeg.y, -90.0f, 90.0f)) {
    m_planet.get().properties.rotation.y = glm::radians(m_rotDeg.y);
    needUpdate = true;
  }

  if (needUpdate)
    m_planet.get().update();
}

} // namespace TestApp