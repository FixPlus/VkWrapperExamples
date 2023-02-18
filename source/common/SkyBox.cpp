#include "SkyBox.h"
#include "vkw/CommandBuffer.hpp"
#include <vkw/Queue.hpp>
#include <vkw/CommandPool.hpp>

static vkw::NullVertexInputState skybox_state{};

SkyBox::SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass, RenderEngine::ShaderLoaderInterface& shaderLoader) :
        m_device(device),
        m_geometry_layout(device, RenderEngine::GeometryLayout::CreateInfo{&skybox_state,vkw::InputAssemblyStateCreateInfo{},RenderEngine::SubstageDescription{"skybox"}, 1}),
        m_projection_layout(device, RenderEngine::SubstageDescription{.shaderSubstageName="skybox"}, 1),
        m_material_layout(device, RenderEngine::MaterialLayout::CreateInfo{
            RenderEngine::SubstageDescription{
                "skybox",
                {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}}},
            vkw::RasterizationStateCreateInfo{}, std::optional<vkw::DepthTestStateCreateInfo>{}, 1}),
        m_lighting_layout(device, RenderEngine::LightingLayout::CreateInfo{RenderEngine::SubstageDescription{"flat"}, pass, subpass}, 1),
        m_geometry(m_geometry_layout),
        m_projection(m_projection_layout),
        m_lighting(m_lighting_layout),
        m_atmo_buffer(device,
                      VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
        m_material(device, m_material_layout, m_atmo_buffer, outScatterTexture()),
        m_out_scatter_texture(device, shaderLoader, m_atmo_buffer, 2048, 2048)
{
    m_atmo_buffer.map();
    m_atmo_mapped = m_atmo_buffer.mapped().data();
}

void SkyBox::draw(RenderEngine::GraphicsRecordingState &buffer) {

    buffer.setGeometry(m_geometry);
    buffer.setProjection(m_projection);
    buffer.setMaterial(m_material);
    buffer.setLighting(m_lighting);
    buffer.bindPipeline();
    buffer.commands().draw(3, 1);

}

SkyBoxSettings::SkyBoxSettings(TestApp::GUIFrontEnd &gui, SkyBox &skybox, const std::string &title): GUIWindow(gui, TestApp::WindowSettings{.title=title}), m_skybox(skybox) {

}

void SkyBoxSettings::onGui() {
    if(ImGui::CollapsingHeader("Sun")){
        ImGui::ColorEdit4("color", &m_skybox.get().sun.color.x);
        ImGui::SliderFloat("irradiance", &m_skybox.get().sun.params.z, 0.1f, 1000.0f);
        ImGui::SliderFloat("lon", &m_skybox.get().sun.params.x, 0.0f, glm::pi<float>() * 2.0f);
        ImGui::SliderFloat("lat", &m_skybox.get().sun.params.y, 0.0f, glm::pi<float>());
    }
    if(ImGui::CollapsingHeader("Atmosphere")){
        m_need_recompute_outscatter = ImGui::SliderFloat4("K", &m_skybox.get().atmosphere.K.x, 0.0f, 1.0f) || m_need_recompute_outscatter;
        m_need_recompute_outscatter = ImGui::SliderFloat("H0", &m_skybox.get().atmosphere.params.z, 0.0f, 1.0f) || m_need_recompute_outscatter;
        m_need_recompute_outscatter = ImGui::SliderFloat("Atmosphere height", &m_skybox.get().atmosphere.params.y, 0.0f, 10000.0f) || m_need_recompute_outscatter;
        m_need_recompute_outscatter = ImGui::SliderFloat("g", &m_skybox.get().atmosphere.params.w, -1.0f, 0.0f) || m_need_recompute_outscatter;
        m_need_recompute_outscatter = ImGui::SliderInt("samples", &m_skybox.get().atmosphere.samples, 5, 50) || m_need_recompute_outscatter;
    }
}

SkyBox::OutScatterTexture::OutScatterTexture(vkw::Device &device, RenderEngine::ShaderLoaderInterface &shaderLoader, vkw::UniformBuffer<Atmosphere> const& atmo,
                                             uint32_t psiRate, uint32_t heightRate):
                                             vkw::Image<vkw::COLOR, vkw::I2D>{device.getAllocator(),
                                                               VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY, .requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
                                                               VK_FORMAT_R32G32B32A32_SFLOAT, heightRate, psiRate, 1, 1, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT},
                                             RenderEngine::ComputeLayout(device,
                                                                         shaderLoader,
                                                                         RenderEngine::SubstageDescription{
                                                                            .shaderSubstageName="atmosphere_outscatter",
                                                                            .setBindings={{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE}}}, 1),
                                             RenderEngine::Compute(static_cast<RenderEngine::ComputeLayout&>(*this)),
                                             m_view(device, *this, format()){
    VkComponentMapping mapping{};
    mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    auto& scatterView = m_view;
    set().write(0, atmo);
    set().writeStorageImage(1, scatterView);

    auto queue = device.anyComputeQueue();
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue.family().index()};
    auto transferBuffer = vkw::PrimaryCommandBuffer{commandPool};

    transferBuffer.begin(0);

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = vkw::AllocatedImage::operator VkImage_T *();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = 1;
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = 0;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      {&transitLayout1, 1});

    transferBuffer.end();

    queue.submit(transferBuffer);

    queue.waitIdle();

}

void SkyBox::OutScatterTexture::recompute(vkw::Device& device) {
    auto queue = device.anyComputeQueue();
    auto commandPool = vkw::CommandPool{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue.family().index()};
    auto transferBuffer = vkw::PrimaryCommandBuffer{commandPool};

    transferBuffer.begin(0);

    VkImageMemoryBarrier transitLayout1{};
    transitLayout1.image = vkw::AllocatedImage::operator VkImage_T *();
    transitLayout1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transitLayout1.pNext = nullptr;
    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transitLayout1.subresourceRange.baseArrayLayer = 0;
    transitLayout1.subresourceRange.baseMipLevel = 0;
    transitLayout1.subresourceRange.layerCount = 1;
    transitLayout1.subresourceRange.levelCount = 1;
    transitLayout1.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                      {&transitLayout1, 1});

    dispatch(transferBuffer, width() / 16, height() / 16, 1);


    transitLayout1.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transitLayout1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    transitLayout1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transitLayout1.dstAccessMask = 0;
    transitLayout1.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    transferBuffer.imageMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      {&transitLayout1, 1});
    transferBuffer.end();

    queue.submit(transferBuffer);

    queue.waitIdle();

}
