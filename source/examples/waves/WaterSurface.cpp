#include "WaterSurface.h"


WaterSurface::WaterSurface(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                           vkw::DescriptorSetLayout const &globalLayout, TestApp::ShaderLoader &loader)
        : m_descriptor_layout(device,
                              {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}),
          m_pipeline_layout(device, {globalLayout, m_descriptor_layout},
                            {VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT &
                                                             VK_SHADER_STAGE_FRAGMENT_BIT, .offset=0, .size=
                            sizeof(glm::vec2) + sizeof(float) + 4 * sizeof(int)}}),
          m_vertex_shader(loader.loadVertexShader("waves")),
          m_fragment_shader(loader.loadFragmentShader("waves")),
          m_pipeline(m_compile_pipeline(device, pass, subpass, loader)),
          m_pool(device, 1,
                 {VkDescriptorPoolSize{.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount=1}}),
          m_set(m_pool, m_descriptor_layout),
          m_buffer(device, 2 * TILE_DIM * TILE_DIM, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY},
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          m_sampler(m_create_sampler(device)),
          m_ubo(device,
                VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
          m_ubo_mapped(m_ubo.map()),
          m_device(device) {
    m_set.write(0, m_ubo);
    std::vector<PrimitiveAttrs> attrs{};

    attrs.reserve((TILE_DIM + 1) * (TILE_DIM + 1));
    for (int j = 0; j <= TILE_DIM; ++j)
        for (int i = 0; i <= TILE_DIM; ++i) {
            PrimitiveAttrs attr{};
            attr.pos = glm::vec3((float) i * TILE_SIZE / (float) (TILE_DIM), 0.0f,
                                 (float) j * TILE_SIZE / (float) (TILE_DIM));
            attrs.push_back(attr);
        }
    m_buffer = TestApp::createStaticBuffer<vkw::VertexBuffer<PrimitiveAttrs>, PrimitiveAttrs>(device, attrs.begin(),
                                                                                              attrs.end());
}

void WaterSurface::draw(vkw::CommandBuffer &buffer, GlobalLayout const &globalLayout) {
    struct PushConstantBlock {
        glm::vec2 translate;
        float scale = 1.0f;
        float waveEnable[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    } constants;
    buffer.bindGraphicsPipeline(m_pipeline);
    buffer.bindDescriptorSets(m_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, globalLayout.set(), 0);
    buffer.bindDescriptorSets(m_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, m_set, 1);
    buffer.bindVertexBuffer(m_buffer, 0, 0);
    int cascadeIndex = 0;


    glm::vec3 center = globalLayout.camera().position();

    m_totalTiles = 0;

    for (int k = 0; k < cascades; ++k)
        for (int i = -2; i < 2; ++i)
            for (int j = -2; j < 2; ++j) {

                // Discard 'inner' tiles
                if (k != 0 && ((i == 0 || i == -1) && (j == 0 || j == -1)))
                    continue;

                auto scale = static_cast<float>(1u << k);
                glm::vec3 tileTranslate =
                        glm::vec3(center.x, 0.0f, center.z) + glm::vec3(i * TILE_SIZE, 0.0f, j * TILE_SIZE) * scale;

                // Discard tiles that do not appear in camera frustum
                if (globalLayout.camera().offBounds(tileTranslate + glm::vec3{-2.0f, -5.0f, -2.0f}, tileTranslate +
                                                                                                    glm::vec3{
                                                                                                            TILE_SIZE *
                                                                                                            scale +
                                                                                                            2.0f, 5.0f,
                                                                                                            TILE_SIZE *
                                                                                                            scale +
                                                                                                            2.0f}))
                    continue;

                auto &indexBuffer = m_full_tile(0);

                constants.translate = glm::vec2(tileTranslate.x, tileTranslate.z);
                constants.scale = scale;

                auto cellSize = TILE_SIZE / TILE_DIM * scale;

                for (int wave = 0; wave < 4; ++wave) {
                    auto wavelength = glm::length(glm::vec2(ubo.waves[wave].x, ubo.waves[wave].y));
                    constants.waveEnable[wave] = glm::clamp(wavelength / 2.0f - cellSize, 0.0f, 1.0f);
                }
                buffer.bindIndexBuffer(indexBuffer, 0);

                buffer.pushConstants(m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, constants);

                buffer.drawIndexed(indexBuffer.size(), 1, 0, 0);
                m_totalTiles++;
            }
}


vkw::GraphicsPipeline
WaterSurface::m_compile_pipeline(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                                 TestApp::ShaderLoader &loader) {
    vkw::GraphicsPipelineCreateInfo createInfo{pass, subpass, m_pipeline_layout};
#if 1
    static vkw::VertexInputStateCreateInfo<vkw::per_vertex<PrimitiveAttrs, 0>> vertexInputStateCreateInfo{};
    createInfo.addVertexInputState(vertexInputStateCreateInfo);
#endif
    createInfo.addVertexShader(m_vertex_shader);
    createInfo.addFragmentShader(m_fragment_shader);
    createInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    createInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    vkw::DepthTestStateCreateInfo depthTestStateCreateInfo{VK_COMPARE_OP_LESS, true};
    createInfo.addDepthTestState(depthTestStateCreateInfo);
    vkw::RasterizationStateCreateInfo rasterizationStateCreateInfo{false, false, VK_POLYGON_MODE_FILL,
                                                                   VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE};
    createInfo.addRasterizationState(rasterizationStateCreateInfo);


    return {device, createInfo};
}


vkw::IndexBuffer<VK_INDEX_TYPE_UINT32> const &WaterSurface::m_full_tile(uint32_t innerCascade) {
    if (m_full_tiles.contains(innerCascade))
        return m_full_tiles.at(innerCascade);
    uint32_t cascadeSparsity = (1u << innerCascade);
    uint32_t cascadeDim = TILE_DIM / cascadeSparsity;
    if (cascadeDim < 4u)
        throw std::runtime_error("Too small cascade dimension(" + std::to_string(cascadeDim) + "). Cascade index: " +
                                 std::to_string(innerCascade));

    std::vector<uint32_t> indices;
    indices.reserve((cascadeDim) * (cascadeDim) * 6u);
    for (int i = 0; i < cascadeDim; ++i)
        for (int j = 0; j < cascadeDim; ++j) {
            indices.push_back(cascadeSparsity * i + cascadeSparsity * (TILE_DIM + 1) * j);
            indices.push_back(cascadeSparsity * (i + 1) + cascadeSparsity * (TILE_DIM + 1) * j);
            indices.push_back(cascadeSparsity * i + cascadeSparsity * (TILE_DIM + 1) * (j + 1));
            indices.push_back(cascadeSparsity * (i + 1) + cascadeSparsity * (TILE_DIM + 1) * (j + 1));
            indices.push_back(cascadeSparsity * i + cascadeSparsity * (TILE_DIM + 1) * (j + 1));
            indices.push_back(cascadeSparsity * (i + 1) + cascadeSparsity * (TILE_DIM + 1) * j);
        }

    return m_full_tiles.emplace(innerCascade,
                                TestApp::createStaticBuffer<vkw::IndexBuffer<VK_INDEX_TYPE_UINT32>, uint32_t>(m_device,
                                                                                                              indices.begin(),
                                                                                                              indices.end())).first->second;
}

vkw::Sampler WaterSurface::m_create_sampler(vkw::Device &device) {
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.maxLod = 1.0f;
    createInfo.minLod = 0.0f;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    return vkw::Sampler{device, createInfo};

}
