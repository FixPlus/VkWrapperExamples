#include "Fractal.h"
#include "Utils.h"

static vkw::NullVertexInputState skybox_state{};

TestApp::Fractal::Fractal(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                          RenderEngine::TextureLoader &textureLoader, uint32_t texWidth, uint32_t texHeight) :
        m_geom_layout(device,
                      RenderEngine::GeometryLayout::CreateInfo{.vertexInputState=&skybox_state, .substageDescription={.shaderSubstageName="skybox"}}),
        m_geometry(m_geom_layout),
        m_projection_layout(device, RenderEngine::SubstageDescription{.shaderSubstageName="skybox"}, 1),
        m_projection(m_projection_layout),
        m_material_layout(device,
                          RenderEngine::MaterialLayout::CreateInfo{.substageDescription={.shaderSubstageName="fractal", .setBindings={{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                                                                                                      {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}}, .maxMaterials=1}),
        m_material(m_material_layout, device, textureLoader),
        m_offscreen_lighting_layout(device,
                                    RenderEngine::LightingLayout::CreateInfo{.substageDescription={.shaderSubstageName="colorDepth"}, .pass=m_offscreenPass, .subpass=0},
                                    1),
        m_offscreen_lighting(m_offscreen_lighting_layout),
        m_offscreenPass(device),
        m_offscreenBuffer(std::make_unique<OffscreenBuffer>(device, m_offscreenPass, texWidth, texHeight)),
        m_device(device),
        m_filter_layout(device, RenderEngine::MaterialLayout::CreateInfo{.substageDescription={.shaderSubstageName="depthFilter", .setBindings={{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                                                                                                                            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                                                                                                                                                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}}, .maxMaterials=1}),
        m_filter_material(m_filter_layout, device, *m_offscreenBuffer),
        m_lighting_layout(device, RenderEngine::LightingLayout::CreateInfo{.substageDescription={.shaderSubstageName="flat"}, .pass=pass, .subpass=subpass},
                          1),
        m_lighting(m_lighting_layout){

}

void TestApp::Fractal::drawOffscreen(vkw::PrimaryCommandBuffer& buffer, RenderEngine::GraphicsPipelinePool& pool) {

    std::array<VkClearValue, 2> values{};
    values.at(0).color = {0.1f, 0.0f, 0.0f, 0.0f};
    values.at(1).depthStencil.depth = 1.0f;
    values.at(1).depthStencil.stencil = 0.0f;


    VkViewport viewport;

    viewport.height = m_offscreenBuffer.get()->extents().height;
    viewport.width = m_offscreenBuffer.get()->extents().width;
    viewport.x = viewport.y = 0.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor;
    scissor.extent.width = m_offscreenBuffer.get()->extents().width;
    scissor.extent.height = m_offscreenBuffer.get()->extents().height;
    scissor.offset.x = 0;
    scissor.offset.y = 0;

    auto renderArea = m_offscreenBuffer->getFullRenderArea();

    buffer.beginRenderPass(m_offscreenPass, *m_offscreenBuffer, renderArea, false, values.size(), values.data());

    buffer.setScissors({scissor});
    buffer.setViewports({viewport});

    auto recorder = RenderEngine::GraphicsRecordingState{buffer, pool};

    recorder.setGeometry(m_geometry);
    recorder.setProjection(m_projection);
    recorder.setMaterial(m_material);
    recorder.setLighting(m_offscreen_lighting);
    recorder.bindPipeline();

    recorder.commands().draw(3, 1, 0, 0);

    buffer.endRenderPass();

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
    m_filter_material.update(filter);
}

void TestApp::Fractal::resizeOffscreenBuffer(uint32_t width, uint32_t height) {
    m_offscreenBuffer = std::make_unique<OffscreenBuffer>(m_device.get(), m_offscreenPass, width, height);
    m_filter_material.updateTargetViews(*m_offscreenBuffer);
    m_filter_material.rewriteOffscreenTextures(*m_offscreenBuffer, m_device.get());
}

void TestApp::Fractal::draw(RenderEngine::GraphicsRecordingState& recorder) {

    recorder.setGeometry(m_geometry);
    recorder.setProjection(m_projection);
    recorder.setMaterial(m_filter_material);
    recorder.setLighting(m_lighting);

    recorder.bindPipeline();

    recorder.commands().draw(3, 1, 0, 0);
}

TestApp::Fractal::FractalMaterial::FractalMaterial(RenderEngine::MaterialLayout &parent, vkw::Device &device,
                                                   RenderEngine::TextureLoader &textureLoader) :
        RenderEngine::Material(parent),
        m_texture(textureLoader.loadTexture("image", 10)),
        m_sampler_with_mips(TestApp::createDefaultSampler(device, 10)),
        m_sampler(TestApp::createDefaultSampler(device)),
        m_buffer(device,
                 VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
        m_mapped(m_buffer.map()),
        m_device(device),
        m_texture_view(device, m_texture, m_texture.format(), 0u, m_texture.mipLevels()){

    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    set().write(1, m_texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
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

    set().write(1, m_texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler_mode ? m_sampler_with_mips : m_sampler);

}

TestApp::FractalSettings::FractalSettings(TestApp::GUIFrontEnd &gui, TestApp::Fractal &fractal) :
        GUIWindow(gui, WindowSettings{.title="Fractal", .autoSize=true}), m_fractal(fractal) {}

void TestApp::FractalSettings::onGui() {
    ImGui::SliderFloat("Mutate", &m_fractal.get().ubo.params.x, 0.0f, 2.0f);
    if (ImGui::Checkbox("Use mipmaps", &m_sampler_mode)) {
        m_fractal.get().switchSamplerMode();
    }
    if(ImGui::Checkbox("FXAA", &m_fxaa_on)){
        m_fractal.get().filter.distance = m_fxaa_on ? 0.0f : 1.0f;
    }
}

TestApp::Fractal::RenderPassAttachments::RenderPassAttachments(vkw::Device &device) :
        m_attachments({vkw::AttachmentDescription{VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                  VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                  VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                       {VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}) {


}

TestApp::Fractal::RenderPass::RenderPass(vkw::Device &device) : RenderPassAttachments(device),
                                                                vkw::RenderPass(device, m_compileRenderPassInfo()) {

}

vkw::RenderPassCreateInfo TestApp::Fractal::RenderPass::m_compileRenderPassInfo() {
    auto subpassDescription = vkw::SubpassDescription{};
    subpassDescription.addColorAttachment(colorAttachment(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    subpassDescription.addColorAttachment(depthAttachment(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    auto inputDependency = vkw::SubpassDependency{};
    inputDependency.setDstSubpass(subpassDescription);
    inputDependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    inputDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    inputDependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    inputDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    inputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    auto outputDependency = vkw::SubpassDependency{};
    outputDependency.setSrcSubpass(subpassDescription);
    outputDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    outputDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    outputDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    outputDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    outputDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    return vkw::RenderPassCreateInfo({{colorAttachment(), depthAttachment()},
                                      {subpassDescription},
                                      {inputDependency,   outputDependency}});
}

TestApp::Fractal::OffscreenBufferImages::OffscreenBufferImages(vkw::Device &device, uint32_t width, uint32_t height) :
        m_colorTarget(device.getAllocator(),
                      VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                      VK_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        m_depthTarget(device.getAllocator(),
                      VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                      VK_FORMAT_R32_SFLOAT, width, height, 1, 1, 1,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        m_targetViews({vkw::ImageView<vkw::COLOR, vkw::V2DA>{device,  m_colorTarget, m_colorTarget.format()}, {device,  m_depthTarget, m_depthTarget.format()}}),
        m_targetRefViews({&m_targetViews.at(0), &m_targetViews.at(1)}){

    doTransitLayout(m_colorTarget, device, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    doTransitLayout(m_depthTarget, device, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

}

TestApp::Fractal::OffscreenBuffer::OffscreenBuffer(vkw::Device &device, TestApp::Fractal::RenderPass &pass,
                                                   uint32_t width, uint32_t height) :
         OffscreenBufferImages(device,width,height),
         vkw::FrameBuffer(device,
                          pass,
                          VkExtent2D{width,height},
                          {targetViews().begin(), targetViews().end()}) {
}

TestApp::Fractal::FilterMaterial::FilterMaterial(RenderEngine::MaterialLayout &parent, vkw::Device &device,
                                                 TestApp::Fractal::OffscreenBuffer &offscreenBuffer):
                                                 RenderEngine::Material(parent),
                                                 m_buffer(device,
                                                          VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                                                  .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                                  m_mapped(m_buffer.map()),
                                                 m_sampler(TestApp::createDefaultSampler(device)),
                                                 m_colorTargetView(device, offscreenBuffer.colorTarget(), offscreenBuffer.colorTarget().format()),
                                                 m_depthTargetView(device, offscreenBuffer.depthTarget(), offscreenBuffer.depthTarget().format()),
                                                 m_device(device){

    set().write(0, m_buffer);
    rewriteOffscreenTextures(offscreenBuffer, device);
}

void TestApp::Fractal::FilterMaterial::update(const TestApp::Fractal::FilterUBO &ubo) {
    *m_mapped = ubo;
    m_buffer.flush();
}

void TestApp::Fractal::FilterMaterial::rewriteOffscreenTextures(TestApp::Fractal::OffscreenBuffer &offscreenBuffer, vkw::Device& device) {
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    set().write(1, m_colorTargetView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
    set().write(2, m_depthTargetView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_sampler);
}

void TestApp::Fractal::FilterMaterial::updateTargetViews(TestApp::Fractal::OffscreenBuffer &offscreenBuffer) {
    m_colorTargetView = vkw::ImageView<vkw::COLOR, vkw::V2D>(m_device, offscreenBuffer.colorTarget(), offscreenBuffer.colorTarget().format());
    m_depthTargetView = vkw::ImageView<vkw::COLOR, vkw::V2D>(m_device, offscreenBuffer.depthTarget(), offscreenBuffer.depthTarget().format());
}
