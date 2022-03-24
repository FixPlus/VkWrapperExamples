#include "RenderEngine/Pipelines/Material.h"
#include "RenderEngine/RecordingState.h"

namespace RenderEngine {


    MaterialLayout::MaterialLayout(vkw::Device &device, const MaterialLayout::CreateInfo &createInfo) : PipelineStageLayout(device, createInfo.substageDescription, createInfo.maxMaterials),
    m_rasterizationState(createInfo.rasterizationState), m_depthTestState(createInfo.depthTestState){
    }

    void Material::bind(GraphicsRecordingState &state) const {
        state.setMaterial(*this);
    }
}