#include "RenderEngine/Pipelines/Material.h"
#include "RenderEngine/RecordingState.h"

namespace RenderEngine {

MaterialLayout::MaterialLayout(vkw::Device &device,
                               ShaderLoaderInterface &loader,
                               const MaterialLayout::CreateInfo &createInfo)
    : PipelineStageLayout(device, loader, createInfo.substageDescription, 2,
                          ".mt.frag", createInfo.maxMaterials),
      m_rasterizationState(createInfo.rasterizationState),
      m_depthTestState(createInfo.depthTestState) {}

void Material::bind(GraphicsRecordingState &state) const {
  state.setMaterial(*this);
}
} // namespace RenderEngine