#ifndef TESTAPP_GEOMETRY_H
#define TESTAPP_GEOMETRY_H

#include "vkw/Common.hpp"
#include <string>
#include <vkw/DescriptorSet.hpp>
#include <vkw/DescriptorPool.hpp>
#include <vkw/CommandBuffer.hpp>
#include <vkw/Pipeline.hpp>
#include <set>
#include "RenderEngine/Pipelines/PipelineStage.h"

namespace RenderEngine {

    class Geometry;

    class GeometryLayout : public PipelineStageLayout {
    public:
        struct CreateInfo {
            vkw::VertexInputStateCreateInfoBase const *vertexInputState;
            vkw::InputAssemblyStateCreateInfo inputAssemblyState{};
            SubstageDescription substageDescription;
            uint32_t maxGeometries = 0;

        };

        GeometryLayout(vkw::Device &device, CreateInfo const &createInfo);

        GeometryLayout(GeometryLayout &&another) noexcept:
                m_inputAssemblyState(another.m_inputAssemblyState),
                m_vertexInputState(another.m_vertexInputState),
                PipelineStageLayout(std::move(another)){

        }
        GeometryLayout &operator=(GeometryLayout &&another) noexcept {
            m_vertexInputState = another.m_vertexInputState;
            m_inputAssemblyState = another.m_inputAssemblyState;
            PipelineStageLayout::operator=(std::move(another));
            return *this;
        }

        vkw::VertexInputStateCreateInfoBase const *vertexInputState() const {
            return &m_vertexInputState;
        }

        vkw::InputAssemblyStateCreateInfo const &inputAssemblyState() const {
            return m_inputAssemblyState;
        }

    private:
        vkw::VertexInputStateCreateInfoBase m_vertexInputState;
        vkw::InputAssemblyStateCreateInfo m_inputAssemblyState;
    };

    class GraphicsRecordingState;

    class Geometry : public PipelineStage<GeometryLayout> {
    public:
        explicit Geometry(GeometryLayout &parent) : PipelineStage(parent) {
        };

        virtual void bind(GraphicsRecordingState &state) const;
    };

}
#endif //TESTAPP_GEOMETRY_H
