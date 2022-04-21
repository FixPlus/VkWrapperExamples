#include "Grid.h"
#include "common/Utils.h"

namespace TestApp {
    vkw::VertexInputStateCreateInfo<vkw::per_vertex<Grid::PrimitiveAttrs, 0>> Grid::m_vertexInputStateCreateInfo{};

    GridSettings::GridSettings(GUIFrontEnd &gui, Grid &grid, const std::string &title): GUIWindow(gui, WindowSettings{.title=title}),
                                                                                        m_grid(grid) {

    }

    void GridSettings::onGui() {
        auto& grid = m_grid.get();
        if(!ImGui::CollapsingHeader("Tile grid settings"))
            return;
        ImGui::SliderInt("Tile cascades", &grid.cascades, 1, 15);
        ImGui::SliderInt("Cascade power", &grid.cascadePower, 1, 4);
        ImGui::SliderFloat("Tile scale", &grid.tileScale, 0.1f, 10.0f);
        ImGui::SliderFloat("Elevation scale", &grid.elevationScale, 0.0f, 1.0f);
        ImGui::Checkbox("Camera aligned", &grid.cameraAligned);
        ImGui::Checkbox("Enabled", &m_enabled);
        ImGui::Text("Total tiles: %d", grid.totalTiles());
    }
}

TestApp::Grid::Grid(vkw::Device &device, bool cameraAligned):
        m_device(device),
        m_buffer(device,
                 2 * TILE_DIM * TILE_DIM,
                 VmaAllocationCreateInfo{.usage=VMA_MEMORY_USAGE_GPU_ONLY},
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        cameraAligned(cameraAligned){

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
    m_buffer = TestApp::createStaticBuffer<vkw::VertexBuffer<PrimitiveAttrs>, PrimitiveAttrs>(
            device, attrs.begin(),
            attrs.end());

}

void TestApp::Grid::m_addConnectingEdge(TestApp::Grid::ConnectSide side, std::vector<uint32_t> &indices) {
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

vkw::IndexBuffer<VK_INDEX_TYPE_UINT32> const &TestApp::Grid::m_full_tile(TestApp::Grid::ConnectSide side) {
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

void TestApp::Grid::draw(RenderEngine::GraphicsRecordingState &buffer, const GlobalLayout &globalLayout) {
    struct PushConstantBlock {
        glm::vec2 translate;
        float scale = 1.0f;
        float cellSize;
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

    preDraw(buffer, globalLayout);

    buffer.commands().bindVertexBuffer(m_buffer, 0, 0);

    int cascadeIndex = 0;



    glm::vec3 center = globalLayout.camera().position();

    auto baseTileSize = tileScale * TILE_SIZE;
    if(!cameraAligned){
        center = {glm::floor(center.x / (2.0f * baseTileSize)) * 2.0f * baseTileSize + baseTileSize, center.y, glm::floor(center.z / (2.0f * baseTileSize)) * 2.0f * baseTileSize + baseTileSize};
    }

    m_totalTiles = 0;

    float elevationFactor = 1.0f + glm::abs(center.y) * elevationScale;
    if(!cameraAligned)
        elevationFactor = 1.0f;

    auto heightBoundsL = heightBounds();

    int cascadeHalfSize = (int)(glm::pow(2.0f, cascadePower));

    for (int k = 0; k < cascades; ++k)
        for (int i = -cascadeHalfSize; i < cascadeHalfSize; ++i)
            for (int j = -cascadeHalfSize; j < cascadeHalfSize; ++j) {
                ConnectSide cside = ConnectSide::NO_CONNECT;

                // Discard 'inner' tiles
                bool iInner = (i < cascadeHalfSize / 2 && i >= -cascadeHalfSize / 2);
                bool jInner = (j < cascadeHalfSize / 2 && j >= -cascadeHalfSize / 2);
                if (k != 0 && (iInner && jInner))
                    continue;
                if (k != 0) {
                    if (iInner)
                        if (j == cascadeHalfSize / 2)
                            cside = ConnectSide::SOUTH;
                        else if (j == (-cascadeHalfSize / 2 - 1))
                            cside = ConnectSide::NORTH;

                    if (jInner)
                        if (i == cascadeHalfSize / 2)
                            cside = ConnectSide::WEST;
                        else if (i == (-cascadeHalfSize / 2 - 1))
                            cside = ConnectSide::EAST;
                }
                auto scale = static_cast<float>(1u << k) * elevationFactor * tileScale;
                glm::vec3 tileTranslate =
                        glm::vec3(center.x, 0.0f, center.z) + glm::vec3(i * TILE_SIZE, 0.0f, j * TILE_SIZE) * scale;

                // Discard tiles that do not appear in camera frustum
                if (globalLayout.camera().offBounds(tileTranslate + glm::vec3{-2.0f, heightBoundsL.first, -2.0f}, tileTranslate +
                                                                                                    glm::vec3{
                                                                                                            TILE_SIZE *
                                                                                                            scale +
                                                                                                            2.0f, heightBoundsL.second,
                                                                                                            TILE_SIZE *
                                                                                                            scale +
                                                                                                            2.0f}))
                    continue;


                auto &indexBuffer = m_full_tile(cside);

                constants.translate = glm::vec2(tileTranslate.x, tileTranslate.z);
                constants.scale = scale;
                constants.cellSize = scale * TILE_SIZE * tileScale / (float)(TILE_DIM);

                buffer.commands().bindIndexBuffer(indexBuffer, 0);

                buffer.pushConstants(constants, VK_SHADER_STAGE_VERTEX_BIT, 0);

                buffer.commands().drawIndexed(indexBuffer.size(), 1, 0, 0);

                m_totalTiles++;
            }

}
