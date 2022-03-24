#include "WaterSurface.h"


static vkw::VertexInputStateCreateInfo<vkw::per_vertex<WaterSurface::PrimitiveAttrs, 0>> vertexInputStateCreateInfo{};


void WaterSurface::draw(RenderEngine::GraphicsRecordingState &buffer, GlobalLayout const &globalLayout) {
    struct PushConstantBlock {
        glm::vec2 translate;
        float scale = 1.0f;
        float waveEnable[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    } constants;

#if 0
    if (wireframe)
        buffer.bindGraphicsPipeline(m_wireframe_pipeline);
    else
        buffer.bindGraphicsPipeline(m_pipeline);


    buffer.bindDescriptorSets(m_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, globalLayout.set(), 0);
    buffer.bindDescriptorSets(m_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, m_set, 1);
    buffer.bindVertexBuffer();
#endif
    buffer.setGeometry(m_geometry);
    buffer.bindPipeline();
    buffer.commands().bindVertexBuffer(m_buffer, 0, 0);

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

                buffer.commands().bindIndexBuffer(indexBuffer, 0);

                buffer.pushConstants(constants, VK_SHADER_STAGE_VERTEX_BIT, 0);

                buffer.commands().drawIndexed(indexBuffer.size(), 1, 0, 0);

                m_totalTiles++;
            }
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

WaterSurface::WaterSurface(vkw::Device &device) :RenderEngine::GeometryLayout(device,
                                                                                      RenderEngine::GeometryLayout::CreateInfo{.vertexInputState = &vertexInputStateCreateInfo, .substageDescription={.shaderSubstageName="waves",
                                                                                              .setBindings = {vkw::DescriptorSetLayoutBinding{0,
                                                                                                                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}}, .pushConstants={
                                                                                                      VkPushConstantRange{.stageFlags=VK_SHADER_STAGE_VERTEX_BIT, .offset=0, .size=
                                                                                                      7 * sizeof(float)}}}, .maxGeometries=1}),
                                                         m_geometry(device, *this),
                                                                                m_device(device),
                                                                                m_buffer(device,
                                                                                         2 * TILE_DIM * TILE_DIM,
                                                                                         VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY},
                                                                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT) {

    std::vector<WaterSurface::PrimitiveAttrs> attrs{};

    attrs.reserve((TILE_DIM + 1) * (TILE_DIM + 1));
    for (int j = 0; j <= TILE_DIM; ++j)
        for (int i = 0; i <= TILE_DIM; ++i) {
            WaterSurface::PrimitiveAttrs attr{};
            attr.pos = glm::vec3((float) i * TILE_SIZE / (float) (TILE_DIM), 0.0f,
                                 (float) j * TILE_SIZE / (float) (TILE_DIM));
            attrs.push_back(attr);
        }

    for (int j = 0; j <= TILE_DIM; ++j) {
        WaterSurface::PrimitiveAttrs attr{};
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
    m_buffer = TestApp::createStaticBuffer<vkw::VertexBuffer<WaterSurface::PrimitiveAttrs>, WaterSurface::PrimitiveAttrs>(
            device, attrs.begin(),
            attrs.end());

}

WaterSurface::Geometry::Geometry(vkw::Device &device, WaterSurface &surface): RenderEngine::Geometry(surface), m_ubo(device,
                                                                                    VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU, .requiredFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}),
                                                                              m_ubo_mapped(m_ubo.map()){
    set().write(0, m_ubo);
}

WaterMaterial::Material::Material(vkw::Device& device, WaterMaterial& waterMaterial): RenderEngine::Material(waterMaterial),  m_buffer(device, VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_CPU_TO_GPU}),
                                                                                      m_mapped(m_buffer.map()){
    set().write(0, m_buffer);
}
