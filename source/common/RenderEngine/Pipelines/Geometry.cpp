#include "Geometry.h"
#include "RenderEngine/RecordingState.h"

namespace RenderEngine {


    GeometryLayout::GeometryLayout(vkw::Device &device, const GeometryLayout::CreateInfo &createInfo) : PipelineStageLayout(device, createInfo.substageDescription, createInfo.maxGeometries),
                                                                                                        m_vertexInputState(*createInfo.vertexInputState),
                                                                                                        m_inputAssemblyState(createInfo.inputAssemblyState){

    }

    void Geometry::bind(GraphicsRecordingState &state) const{
        state.setGeometry(*this);
    }
}