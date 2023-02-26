#ifndef TESTAPP_GRID_H
#define TESTAPP_GRID_H

#include <glm/glm.hpp>
#include <vkw/VertexBuffer.hpp>
#include <vector>
#include <map>
#include "RenderEngine/Pipelines/PipelinePool.h"
#include "GlobalLayout.h"
#include "GUI.h"

namespace TestApp {

class Grid : public vkw::ReferenceGuard{
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
        vkw::StrongReference<vkw::Device> m_device;

        auto m_createVertexState(){
            return std::make_unique<vkw::VertexInputStateCreateInfo<vkw::per_vertex<PrimitiveAttrs, 0>>>();
        }

        virtual void preDraw(RenderEngine::GraphicsRecordingState &buffer) {}
        virtual glm::vec4 arbitraryData(glm::vec2 tileBegin,  glm::vec2 tileEnd) { return glm::vec4{0.0f}; };

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

        vkw::StrongReference<Grid> m_grid;
    private:
        bool m_enabled = true;
    };
}
#endif //TESTAPP_GRID_H
