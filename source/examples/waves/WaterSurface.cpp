#include "WaterSurface.h"


WaterSurface::WaterSurface(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                           vkw::DescriptorSetLayout const &globalLayout, TestApp::ShaderLoader &loader)
        : m_descriptor_layout(device,
                              {vkw::DescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}),
          m_pipeline_layout(device, {globalLayout, m_descriptor_layout},
                            {VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0, .size=
                            sizeof(glm::vec2) + sizeof(float) + 4 * sizeof(int)}}),
          m_vertex_shader(loader.loadVertexShader("waves")),
          m_fragment_shader(loader.loadFragmentShader("waves")),
          m_pipeline(m_compile_pipeline(device, pass, subpass, loader, false)),
          m_wireframe_pipeline(m_compile_pipeline(device, pass, subpass, loader, true)),
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

    for (int j = 0; j <= TILE_DIM; ++j) {
        PrimitiveAttrs attr{};
        float connectVertexPos = ((float) (j) + 0.5f) * TILE_SIZE / (float) (TILE_DIM);
        // North Connect vertices
        attr.pos = glm::vec3(connectVertexPos, 0.0f, TILE_SIZE);
        attrs.push_back(attr);
        // East Connect vertices
        attr.pos = glm::vec3(TILE_SIZE, 0.0f, connectVertexPos);
        attrs.push_back(attr);
        // South Connect vertices
        attr.pos = glm::vec3(connectVertexPos, 0.0f, 0.0f);
        attrs.push_back(attr);
        // West Connect vertices
        attr.pos = glm::vec3(0.0f, 0.0f, connectVertexPos);
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

    if (wireframe)
        buffer.bindGraphicsPipeline(m_wireframe_pipeline);
    else
        buffer.bindGraphicsPipeline(m_pipeline);

    buffer.bindDescriptorSets(m_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, globalLayout.set(), 0);
    buffer.bindDescriptorSets(m_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, m_set, 1);
    buffer.bindVertexBuffer(m_buffer, 0, 0);
    int cascadeIndex = 0;


    glm::vec3 center = globalLayout.camera().position();

    m_totalTiles = 0;

    float elevationFactor = 1.0f + glm::abs(center.y) * elevationScale;

    for (int k = 0; k < cascades; ++k)
        for (int i = -2; i < 2; ++i)
            for (int j = -2; j < 2; ++j) {
                ConnectSide cside = ConnectSide::NO_CONNECT;

                // Discard 'inner' tiles
                if (k != 0 && ((i == 0 || i == -1) && (j == 0 || j == -1)))
                    continue;
                if (k != 0) {
                    if ((i == -1 || i == 0))
                        if (j == 1)
                            cside = ConnectSide::SOUTH;
                        else if (j == -2)
                            cside = ConnectSide::NORTH;

                    if ((j == -1 || j == 0))
                        if (i == 1)
                            cside = ConnectSide::WEST;
                        else if (i == -2)
                            cside = ConnectSide::EAST;
                }
                auto scale = static_cast<float>(1u << k) * elevationFactor * tileScale;
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


                auto &indexBuffer = m_full_tile(cside);

                constants.translate = glm::vec2(tileTranslate.x, tileTranslate.z);
                constants.scale = scale;

                buffer.bindIndexBuffer(indexBuffer, 0);

                buffer.pushConstants(m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, constants);

                buffer.drawIndexed(indexBuffer.size(), 1, 0, 0);
                m_totalTiles++;
            }
}


vkw::GraphicsPipeline
WaterSurface::m_compile_pipeline(vkw::Device &device, vkw::RenderPass &pass, uint32_t subpass,
                                 TestApp::ShaderLoader &loader, bool wireframe) {
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
    vkw::RasterizationStateCreateInfo rasterizationStateCreateInfo{false, false, wireframe ? VK_POLYGON_MODE_LINE
                                                                                           : VK_POLYGON_MODE_FILL,
                                                                   VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE};
    createInfo.addRasterizationState(rasterizationStateCreateInfo);


    return {device, createInfo};
}


vkw::IndexBuffer<VK_INDEX_TYPE_UINT32> const &WaterSurface::m_full_tile(ConnectSide side) {
    int iside = static_cast<int>(side);
    if (m_full_tiles.contains(iside))
        return m_full_tiles.at(iside);

    std::vector<uint32_t> indices;

    indices.reserve((TILE_DIM) * (TILE_DIM) * 6u);


    for (int i = 0; i < TILE_DIM; ++i) {
        if (i == 0 && side == ConnectSide::WEST)
            continue;
        if (i == TILE_DIM - 1 && side == ConnectSide::EAST)
            continue;
        for (int j = 0; j < TILE_DIM; ++j) {
            if (j == TILE_DIM - 1 && side == ConnectSide::NORTH)
                continue;
            if (j == 0 && side == ConnectSide::SOUTH)
                continue;
            indices.push_back(i + (TILE_DIM + 1) * j);
            indices.push_back((i + 1) + (TILE_DIM + 1) * j);
            indices.push_back(i + (TILE_DIM + 1) * (j + 1));
            indices.push_back((i + 1) + (TILE_DIM + 1) * (j + 1));
            indices.push_back(i + (TILE_DIM + 1) * (j + 1));
            indices.push_back((i + 1) + (TILE_DIM + 1) * j);
        }
    }

    if (side != ConnectSide::NO_CONNECT) {
        m_addConnectingEdge(side, indices);
    }

    return m_full_tiles.emplace(iside,
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

void WaterSurface::m_addConnectingEdge(WaterSurface::ConnectSide side, std::vector<uint32_t> &indices) {

    assert(side != ConnectSide::NO_CONNECT && "side must be valid connect side");

    int iside = static_cast<int>(side);

    const int connectVerticesOffset = (TILE_DIM + 1) * (TILE_DIM + 1);

    for (int i = 0; i < TILE_DIM; ++i) {
        int offset = connectVerticesOffset + i * 4 + iside;
        switch (side) {
            case ConnectSide::NORTH:
                indices.push_back(i + (TILE_DIM + 1) * (TILE_DIM - 1));
                indices.push_back(offset);
                indices.push_back(i + (TILE_DIM + 1) * TILE_DIM);


                indices.push_back(i + (TILE_DIM + 1) * (TILE_DIM - 1));
                indices.push_back(i + 1 + (TILE_DIM + 1) * (TILE_DIM - 1));
                indices.push_back(offset);

                indices.push_back(i + 1 + (TILE_DIM + 1) * (TILE_DIM - 1));
                indices.push_back(i + 1 + (TILE_DIM + 1) * TILE_DIM);
                indices.push_back(offset);
                break;
            case ConnectSide::SOUTH:
                indices.push_back(i + (TILE_DIM + 1) * 1);
                indices.push_back(i + (TILE_DIM + 1) * 0);
                indices.push_back(offset);

                indices.push_back(i + (TILE_DIM + 1) * 1);
                indices.push_back(offset);
                indices.push_back(i + 1 + (TILE_DIM + 1) * 1);


                indices.push_back(i + 1 + (TILE_DIM + 1) * 1);
                indices.push_back(offset);
                indices.push_back(i + 1 + (TILE_DIM + 1) * 0);
                break;
            case ConnectSide::EAST:
                indices.push_back(TILE_DIM - 1 + (TILE_DIM + 1) * i);
                indices.push_back(TILE_DIM + (TILE_DIM + 1) * i);
                indices.push_back(offset);

                indices.push_back(TILE_DIM - 1 + (TILE_DIM + 1) * i);
                indices.push_back(offset);
                indices.push_back(TILE_DIM - 1 + (TILE_DIM + 1) * (i + 1));

                indices.push_back(TILE_DIM - 1 + (TILE_DIM + 1) * (i + 1));
                indices.push_back(offset);
                indices.push_back(TILE_DIM + (TILE_DIM + 1) * (i + 1));
                break;
            case ConnectSide::WEST:
                indices.push_back(1 + (TILE_DIM + 1) * i);
                indices.push_back(offset);
                indices.push_back(0 + (TILE_DIM + 1) * i);


                indices.push_back(1 + (TILE_DIM + 1) * i);
                indices.push_back(1 + (TILE_DIM + 1) * (i + 1));
                indices.push_back(offset);

                indices.push_back(1 + (TILE_DIM + 1) * (i + 1));
                indices.push_back(0 + (TILE_DIM + 1) * (i + 1));
                indices.push_back(offset);
                break;
        }
    }
}
