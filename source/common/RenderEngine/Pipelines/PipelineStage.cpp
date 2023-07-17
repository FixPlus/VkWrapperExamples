#include "RenderEngine/Pipelines/PipelineStage.h"
#include "RenderEngine/Pipelines/ShaderLoaderInterface.h"

namespace RenderEngine {

namespace {

std::optional<vkw::SPIRVDescriptorSetInfo>
findStageSet(vkw::SPIRVModuleInfo const &moduleInfo, uint32_t stageSet) {
  auto sets = moduleInfo.sets();
  auto stageSetIt = std::ranges::find_if(
      sets, [&](auto &&set) { return set.index() == stageSet; });
  if (stageSetIt == sets.end())
    return std::nullopt;
  return *stageSetIt;
}

} // namespace

PipelineStageLayout::PipelineStageLayout(vkw::Device &device,
                                         ShaderLoaderInterface &loader,
                                         SubstageDescription desc,
                                         uint32_t stageSet,
                                         std::string_view stagePostfix,
                                         uint32_t maxSets)
    : m_module([&]() {
        /// Module is initialized as a linked combination of all requested
        /// shader stages.
        std::vector<vkw::SPIRVModule> modules;
        for (auto &moduleName : desc.additionalShaderFiles) {
          modules.emplace_back(loader.loadModule(moduleName));
        }
        modules.emplace_back(loader.loadModule(desc.shaderSubstageName +
                                               std::string(stagePostfix)));
        return vkw::SPIRVModule(modules, /* link library */ true);
      }()),
      m_layout([&]() {
        /// Descriptor layout is initialized using reflected info from
        /// shader module.
        auto &moduleInfo = m_module.info();

        boost::container::small_vector<vkw::DescriptorSetLayoutBinding, 4>
            bindings;
        if (auto set = findStageSet(moduleInfo, stageSet)) {
          for (auto &&binding : set.value().bindings()) {
            bindings.emplace_back(binding.index(), binding.descriptorType());
          }
        }
        return vkw::DescriptorSetLayout(device, bindings);
      }()),
      m_pool([&]() -> std::optional<vkw::DescriptorPool> {
        /// Descriptor pool is initialized like Descriptor layout - form
        /// reflected info.
        auto &moduleInfo = m_module.info();
        if (auto set = findStageSet(moduleInfo, stageSet)) {
          boost::container::small_vector<VkDescriptorPoolSize, 3> sizes{};
          for (auto &&binding : set.value().bindings()) {
            auto type = binding.descriptorType();
            auto found = std::find_if(sizes.begin(), sizes.end(),
                                      [type](VkDescriptorPoolSize const &size) {
                                        return size.type == type;
                                      });
            if (found != sizes.end()) {
              found->descriptorCount += maxSets;
            } else {
              sizes.emplace_back(VkDescriptorPoolSize{
                  .type = type, .descriptorCount = maxSets});
            }
          }

          return vkw::DescriptorPool{
              device, maxSets,
              std::span<VkDescriptorPoolSize>{sizes.data(), sizes.size()}};
        } else
          return std::nullopt;
      }()),
      m_description(std::move(desc)) {}

PipelineStageLayout::PipelineStageLayout(PipelineStageLayout &&another) noexcept
    : m_layout(std::move(another.m_layout)), m_pool(std::move(another.m_pool)),
      m_module(std::move(another.m_module)),
      m_description(std::move(another.m_description)),
      m_stages(std::move(another.m_stages)) {
  for (auto &stage : m_stages) {
    stage->m_layout = *this;
  }
}

PipelineStageLayout &
PipelineStageLayout::operator=(PipelineStageLayout &&another) noexcept {
  m_module = std::move(another.m_module);
  m_layout = std::move(another.m_layout);
  m_pool = std::move(another.m_pool);
  m_description = std::move(another.m_description);
  m_stages = std::move(another.m_stages);

  for (auto &stage : m_stages) {
    stage->m_layout = *this;
  }

  return *this;
}
} // namespace RenderEngine