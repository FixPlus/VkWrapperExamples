#include "Geometry.h"
#include "RenderEngine/RecordingState.h"

namespace RenderEngine {

GeometryLayout::GeometryLayout(vkw::Device &device,
                               ShaderLoaderInterface &loader,
                               GeometryLayout::CreateInfo &&createInfo)
    : PipelineStageLayout(device, loader, createInfo.substageDescription, 0,
                          ".gm.vert", createInfo.maxGeometries),
      m_vertexInputState(std::move(createInfo.vertexInputState)),
      m_inputAssemblyState(createInfo.inputAssemblyState) {}

void Geometry::bind(GraphicsRecordingState &state) const {
  state.setGeometry(*this);
}
} // namespace RenderEngine