#include "Fractal.h"
#include "Utils.h"

static vkw::NullVertexInputState skybox_state{};

TestApp::Fractal::Fractal(vkw::Device &device, vkw::RenderPass& pass, uint32_t subpass,
                          RenderEngine::TextureLoader &textureLoader):
        m_geom_layout(device, RenderEngine::GeometryLayout::CreateInfo{.vertexInputState=&skybox_state,.substageDescription={.shaderSubstageName="skybox"}}),
        m_geometry(m_geom_layout),
        m_projection_layout(device, RenderEngine::SubstageDescription{.shaderSubstageName="skybox"}, 1),
        m_projection(m_projection_layout),
        m_material_layout(device, RenderEngine::MaterialLayout::CreateInfo{.substageDescription={.shaderSubstageName="fractal",.setBindings={{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}}, .maxMaterials=1}),
        m_material(m_material_layout, device, textureLoader),
        m_lighting_layout(device, RenderEngine::LightingLayout::CreateInfo{.substageDescription={.shaderSubstageName="flat"}, .pass=pass,.subpass=subpass}, 1),
        m_lighting(m_lighting_layout){

}

void TestApp::Fractal::draw(RenderEngine::GraphicsRecordingState &recorder) {
    recorder.setGeometry(m_geometry);
    recorder.setProjection(m_projection);
    recorder.setMaterial(m_material);
    recorder.setLighting(m_lighting);
    recorder.bindPipeline();

    recorder.commands().draw(3, 1, 0, 0);
}

void TestApp::Fractal::update(const TestApp::CameraPerspective &camera) {
    ubo.near_plane = camera.nearPlane();
    ubo.far_plane = camera.farPlane();
    glm::mat4 view = glm::mat4(1.0f);

    view = glm::rotate(view, glm::radians(camera.psi()),
                         glm::vec3(1.0f, 0.0f, 0.0f));
    view = glm::rotate(view, glm::radians(camera.phi()),
                         glm::vec3(0.0f, 1.0f, 0.0f));
    view = glm::rotate(view, glm::radians(camera.tilt()),
                         glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.invProjView = glm::inverse(camera.projection() * view);
    ubo.cameraPos = glm::vec4(camera.position(), 1.0f);
    m_material.update(ubo);
}

TestApp::Fractal::FractalMaterial::FractalMaterial(RenderEngine::MaterialLayout &parent, vkw::Device &device,
                                                   RenderEngine::TextureLoader &textureLoader):
        RenderEngine::Material(parent),
        m_texture(textureLoader.loadTexture("image", 10)),
        m_sampler_with_mips(TestApp::createDefaultSampler(device, 10)),
        m_sampler(TestApp::createDefaultSampler(device)),
        m_buffer(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU,.requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
        m_mapped(m_buffer.map()),
        m_device(device){

    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto& texView = m_texture.getView<vkw::ColorImageView>(device, m_texture.format(), mapping, 0, m_texture.mipLevels());

    set().write(1, texView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
    set().write(0, m_buffer);
}

void TestApp::Fractal::FractalMaterial::update(const TestApp::Fractal::UBO &ubo) {
    *m_mapped = ubo;
    m_buffer.flush();
}

void TestApp::Fractal::FractalMaterial::switchSamplerMode() {

    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    m_sampler_mode = !m_sampler_mode;

    auto& texView = m_texture.getView<vkw::ColorImageView>(m_device, m_texture.format(), mapping, 0, m_texture.mipLevels());

    set().write(1, texView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler_mode? m_sampler_with_mips : m_sampler);

}

TestApp::FractalSettings::FractalSettings(TestApp::GUIFrontEnd &gui, TestApp::Fractal &fractal):
        GUIWindow(gui, WindowSettings{.title="Fractal",.autoSize=true}), m_fractal(fractal){}

void TestApp::FractalSettings::onGui() {
    ImGui::SliderFloat("Mutate", &m_fractal.get().ubo.params.x, 0.0f, 2.0f);
    if(ImGui::Checkbox("Use mipmaps", &m_sampler_mode)){
        m_fractal.get().switchSamplerMode();
    }
}
