#include "RenderEngine/Pipelines/PipelineStage.h"


namespace RenderEngine {

    vkw::DescriptorPool
    PipelineStageLayout::m_initPool(vkw::Device &device, const SubstageDescription &createInfo, uint32_t maxSets) {
        boost::container::small_vector<VkDescriptorPoolSize, 3> sizes{};


        for (auto &binding: createInfo.setBindings) {
            auto type = binding.type();
            auto found = std::find_if(sizes.begin(), sizes.end(),
                                      [type](VkDescriptorPoolSize const &size) { return size.type == type; });
            if (found != sizes.end()) {
                found->descriptorCount += maxSets;
            } else {
                sizes.emplace_back(VkDescriptorPoolSize{.type=type, .descriptorCount=maxSets});
            }
        }

        return {device, maxSets, std::span<VkDescriptorPoolSize>{sizes.data(), sizes.size()}};
    }

    PipelineStageLayout::PipelineStageLayout(PipelineStageLayout &&another) noexcept: m_layout(
            std::move(another.m_layout)),
                                                                                      m_pool(std::move(another.m_pool)),
                                                                                      m_description(std::move(
                                                                                              another.m_description)),
                                                                                      m_stages(std::move(
                                                                                              another.m_stages)) {
        for (auto &stage: m_stages) {
            stage->m_layout = *this;
        }
    }

    PipelineStageLayout &PipelineStageLayout::operator=(PipelineStageLayout &&another) noexcept {
        m_layout = std::move(another.m_layout);
        m_pool = std::move(another.m_pool);
        m_description = std::move(another.m_description);
        m_stages = std::move(another.m_stages);

        for (auto &stage: m_stages) {
            stage->m_layout = *this;
        }

        return *this;
    }
}