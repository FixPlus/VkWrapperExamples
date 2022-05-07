#ifndef TESTAPP_GRID_H
#define TESTAPP_GRID_H

#include <glm/glm.hpp>
#include <VertexBuffer.hpp>
#include <vector>
#include <map>
#include "RenderEngine/Pipelines/PipelinePool.h"
#include "GlobalLayout.h"
#include "GUI.h"

namespace TestApp {

    class Grid {
    public:
        struct PrimitiveAttrs
                : public vkw::AttributeBase<vkw::VertexAttributeType::VEC3F, vkw::VertexAttributeType::VEC2F> {
            glm::vec3 pos;
            glm::vec2 uv;
        };

        static constexpr uint32_t TILE_DIM = 64;
        static constexpr float TILE_SIZE = 20.0f;

        int cascades = 7;
        float tileScale = 1.0f;
        float elevationScale = 0.1f;
        bool cameraAligned;
        int cascadePower = 1;

        explicit Grid(vkw::Device &device, bool cameraAligned = true);

        int totalTiles() const {
            return m_totalTiles;
        }

        void draw(RenderEngine::GraphicsRecordingState &buffer, glm::vec3 center, Camera const &camera);

    private:
        enum class ConnectSide {
            NORTH = 0,
            EAST = 1,
            SOUTH = 2,
            WEST = 3,
            NO_CONNECT = 4
        };

        static void m_addConnectingEdge(ConnectSide side, std::vector<uint32_t> &indices);

        vkw::IndexBuffer<VK_INDEX_TYPE_UINT32> const &m_full_tile(ConnectSide side);


        int m_totalTiles = 0;

        vkw::VertexBuffer<PrimitiveAttrs> m_buffer;
        std::map<int, vkw::IndexBuffer<VK_INDEX_TYPE_UINT32>> m_full_tiles;

    protected:
        std::reference_wrapper<vkw::Device> m_device;

        static vkw::VertexInputStateCreateInfo<vkw::per_vertex<PrimitiveAttrs, 0>> m_vertexInputStateCreateInfo;

        virtual void preDraw(RenderEngine::GraphicsRecordingState &buffer) {}

        virtual std::pair<float, float> heightBounds() const { return {0.0f, 1.0f}; };
    };


    class GridSettings : public GUIWindow {
    public:
        GridSettings(GUIFrontEnd &gui, Grid &grid, std::string const &title);

        bool enabled() const {
            return m_enabled;
        }

    protected:
        void onGui() override;

        std::reference_wrapper<Grid> m_grid;
    private:
        bool m_enabled = true;
    };
}
#endif //TESTAPP_GRID_H
