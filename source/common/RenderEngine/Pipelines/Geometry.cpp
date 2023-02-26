#include "Geometry.h"
#include "RenderEngine/RecordingState.h"

namespace RenderEngine {


    GeometryLayout::GeometryLayout(vkw::Device &device, GeometryLayout::CreateInfo&& createInfo) : PipelineStageLayout(device, createInfo.substageDescription, createInfo.maxGeometries),
                                                                                                        m_vertexInputState(std::move(createInfo.vertexInputState)),
                                                                                                        m_inputAssemblyState(createInfo.inputAssemblyState){

    }

    void Geometry::bind(GraphicsRecordingState &state) const{
        state.setGeometry(*this);
    }
}