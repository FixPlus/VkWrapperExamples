#include "CommonApp.h"
#include "ErrorCallbackWrapper.h"
#include "SphereMesh.hpp"

using namespace TestApp;

class SimpleGeometryDisplay {
public:
  SimpleGeometryDisplay(vkw::Device &device,
                        RenderEngine::TextureLoader &textureLoader,
                        vkw::RenderPass const &pass, unsigned subpass,
                        Camera const &camera)
      : m_projection(device, camera),
        m_texture(textureLoader.loadTexture("earth_color")),
        m_sampler(device, m_getSamplerCreateInfo()),
        m_solid_material_noCull(device, m_texture, m_sampler, false, false),
        m_solid_material_cull(device, m_texture, m_sampler, false, true),
        m_lighting(device, pass, subpass) {
    if (device.physicalDevice().enabledFeatures().fillModeNonSolid) {
      m_wireframe_material.emplace(device, m_texture, m_sampler, true, false);
    }
  }

  void bind(RenderEngine::GraphicsRecordingState &recorder, bool useWireframe,
            bool enableCulling) const {
    assert(!useWireframe || hasWireframeMaterial() && "Cannot use wireframe");
    recorder.setProjection(m_projection);
    recorder.setMaterial(useWireframe
                             ? m_wireframe_material.value()
                             : (enableCulling ? m_solid_material_cull
                                              : m_solid_material_noCull));
    recorder.setLighting(m_lighting);
  }

  void update(glm::vec3 lightDirection, glm::vec3 lightColor) {
    m_projection.update();
    m_lighting.update(lightDirection, lightColor);
  }

  bool hasWireframeMaterial() const { return m_wireframe_material.has_value(); }

private:
  static VkSamplerCreateInfo m_getSamplerCreateInfo() {
    VkSamplerCreateInfo ret{};
    ret.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ret.pNext = nullptr;
    ret.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ret.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ret.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ret.magFilter = VK_FILTER_LINEAR;
    ret.minFilter = VK_FILTER_LINEAR;

    return ret;
  }
  struct Projection : public RenderEngine::ProjectionLayout,
                      public RenderEngine::Projection {
    Projection(vkw::Device &device, Camera const &camera)
        : RenderEngine::
              ProjectionLayout{device,
                               RenderEngine::SubstageDescription{
                                   .shaderSubstageName = "perspective",
                                   .setBindings =
                                       {vkw::DescriptorSetLayoutBinding(
                                           0,
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)}},
                               1},
          RenderEngine::Projection(
              static_cast<RenderEngine::ProjectionLayout &>(*this)),
          ubo{device,
              VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                      .requiredFlags =
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}},
          camera(camera) {
      set().write(0, ubo);
      ubo.map();
    }
    void update() {
      ubo.mapped().front().first = camera.get().projection();
      ubo.mapped().front().second = camera.get().cameraSpace();
      ubo.flush();
    }

    std::reference_wrapper<Camera const> camera;
    vkw::UniformBuffer<std::pair<glm::mat4, glm::mat4>> ubo;
  } m_projection;
  struct Material : public RenderEngine::MaterialLayout,
                    public RenderEngine::Material {
    Material(vkw::Device &device, vkw::Image<vkw::COLOR, vkw::I2D> const &image,
             vkw::Sampler const &sampler, bool wireframe, bool enableCulling)
        : RenderEngine::MaterialLayout(
              device,
              RenderEngine::MaterialLayout::CreateInfo{
                  RenderEngine::SubstageDescription{
                      "texture",
                      {vkw::DescriptorSetLayoutBinding(
                          0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)},
                      {}},
                  vkw::RasterizationStateCreateInfo(
                      false, false,
                      wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
                      enableCulling ? VK_CULL_MODE_BACK_BIT
                                    : VK_CULL_MODE_NONE),
                  vkw::DepthTestStateCreateInfo(VK_COMPARE_OP_LESS, true), 1}),
          RenderEngine::Material(
              static_cast<RenderEngine::MaterialLayout &>(*this)),
          textureView(device, image, image.format()) {
      set().write(0, textureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  sampler);
    }

    vkw::ImageView<vkw::COLOR, vkw::V2D> textureView;
  };

  vkw::Sampler m_sampler;
  vkw::Image<vkw::COLOR, vkw::I2D> m_texture;
  Material m_solid_material_noCull;
  Material m_solid_material_cull;

  std::optional<Material> m_wireframe_material;

  struct Lighting : public RenderEngine::LightingLayout,
                    public RenderEngine::Lighting {
    Lighting(vkw::Device &device, vkw::RenderPass const &pass, unsigned subpass)
        : RenderEngine::
              LightingLayout{device,
                             RenderEngine::LightingLayout::CreateInfo{
                                 RenderEngine::SubstageDescription{
                                     .shaderSubstageName = "directLight",
                                     .setBindings =
                                         {vkw::DescriptorSetLayoutBinding(
                                             0,
                                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)}},
                                 pass,
                                 subpass,
                                 {}},
                             1},
          RenderEngine::Lighting(
              static_cast<RenderEngine::LightingLayout &>(*this)),
          ubo(device,
              VmaAllocationCreateInfo{
                  .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                  .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}) {
      ubo.map();
      set().write(0, ubo);
    }

    void update(glm::vec3 lightDirection, glm::vec3 lightColor) {
      ubo.mapped().front().first = glm::vec4(lightDirection, 1.0f);
      ubo.mapped().front().second = glm::vec4(lightColor, 1.0f);
      ubo.flush();
    }
    vkw::UniformBuffer<std::pair<glm::vec4, glm::vec4>> ubo;
  } m_lighting;
};

class SimpleGeometryDisplayOptions : public GUIWindow {
public:
  SimpleGeometryDisplayOptions(GUIFrontEnd &gui, SimpleGeometryDisplay &handle)
      : GUIWindow(gui, WindowSettings{.title = "Scene options"}),
        m_handle(handle) {}

  bool wireframeSelected() const { return m_wireframe; }
  bool cullingEnabled() const { return m_culling; }

protected:
  void onGui() override {
    ImGui::Checkbox("wireframe", &m_wireframe);
    ImGui::Checkbox("culling", &m_culling);
    if (!m_handle.get().hasWireframeMaterial())
      m_wireframe = false;
    if (ImGui::SliderFloat3("light direction", &lightDirection.x, -1.0f,
                            1.0f)) {
      if (glm::length(lightDirection) < 0.01f)
        lightDirection.x = 1.0f;
      lightDirection = glm::normalize(lightDirection);
    }
    ImGui::ColorEdit3("light color", &lightColor.x);

    m_handle.get().update(lightDirection, lightColor);
  }

private:
  bool m_wireframe = false;
  bool m_culling = false;
  glm::vec3 lightDirection = glm::vec3{1.0f, 0.0f, 0.0f};
  glm::vec3 lightColor = glm::vec3{1.0f, 1.0f, 1.0f};

  std::reference_wrapper<SimpleGeometryDisplay> m_handle;
};
class PlanetApp : public CommonApp {
public:
  PlanetApp()
      : CommonApp(AppCreateInfo{
            .enableValidation = true,
            .applicationName = "Planet",
            .amendDeviceCreateInfo =
                [](vkw::PhysicalDevice &device) {
                  if (device.isFeatureSupported(
                          vkw::PhysicalDevice::feature::fillModeNonSolid))
                    device.enableFeature(
                        vkw::PhysicalDevice::feature::fillModeNonSolid);
                }}),
        m_mesh(device(), 0, false), m_meshOptions(gui(), m_mesh),
        m_geometryDisplay(device(), textureLoader(), onScreenPass(), 0,
                          window().camera()),
        m_geometryDisplayOptions(gui(), m_geometryDisplay) {}

protected:
  void onPollEvents() override { m_mesh.update(); }
  void onMainPass(vkw::PrimaryCommandBuffer &buffer,
                  RenderEngine::GraphicsRecordingState &recorder) override {
    m_geometryDisplay.bind(recorder,
                           m_geometryDisplayOptions.wireframeSelected(),
                           m_geometryDisplayOptions.cullingEnabled());
    m_mesh.draw(recorder);
  }

private:
  SimpleSphereMesh m_mesh;
  SimpleSphereMeshOptions m_meshOptions;
  SimpleGeometryDisplay m_geometryDisplay;
  SimpleGeometryDisplayOptions m_geometryDisplayOptions;
};

int runPlanet() {
  PlanetApp app;
  app.run();
  return 0;
}

int main() {
  return ErrorCallbackWrapper<decltype(&runPlanet)>::run(&runPlanet);
}