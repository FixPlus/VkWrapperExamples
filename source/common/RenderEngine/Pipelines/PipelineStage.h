#ifndef TESTAPP_PIPELINESTAGE_H
#define TESTAPP_PIPELINESTAGE_H

#include <optional>
#include <set>
#include <string>
#include <vkw/DescriptorPool.hpp>
#include <vkw/DescriptorSet.hpp>
#include <vkw/SPIRVModule.hpp>

namespace RenderEngine {

struct SubstageDescription {
  std::string shaderSubstageName;
  boost::container::small_vector<std::string, 2> additionalShaderFiles;
};

class ShaderLoaderInterface;

class PipelineStageBase;

class PipelineStageLayout : public vkw::ReferenceGuard {
public:
  PipelineStageLayout(vkw::Device &device, ShaderLoaderInterface &loader,
                      SubstageDescription desc, uint32_t stageSet,
                      std::string_view stagePostfix, uint32_t maxSets);

  PipelineStageLayout(PipelineStageLayout &&another) noexcept;
  PipelineStageLayout &operator=(PipelineStageLayout &&another) noexcept;

  vkw::DescriptorSetLayout const &layout() const { return m_layout; }

  SubstageDescription const &description() const { return m_description; }

  auto &module() const { return m_module; }

  virtual ~PipelineStageLayout() = default;

private:
  friend class PipelineStageBase;

  std::set<PipelineStageBase *> m_stages;
  vkw::SPIRVModule m_module;
  vkw::DescriptorSetLayout m_layout;
  std::optional<vkw::DescriptorPool> m_pool;
  SubstageDescription m_description;
};

class PipelineStageBase {
public:
  PipelineStageBase(PipelineStageLayout &layout) : m_layout(layout) {
    if (layout.m_pool)
      m_set.emplace(layout.m_pool.value(), layout.m_layout);
    m_layout.get().m_stages.emplace(this);
  }

  PipelineStageBase(PipelineStageBase &&another) noexcept
      : m_layout(another.m_layout), m_set(std::move(another.m_set)) {
    m_layout.get().m_stages.emplace(this);
  }

  PipelineStageBase &operator=(PipelineStageBase &&another) noexcept {
    m_layout = another.m_layout;
    m_set = std::move(another.m_set);
    m_layout.get().m_stages.emplace(this);
    return *this;
  }

  PipelineStageLayout const &m_get_layout() const { return m_layout; };

  vkw::DescriptorSet const &m_get_set() const { return m_set.value(); };

  vkw::DescriptorSet &m_get_set() { return m_set.value(); };

  bool m_has_set() const { return m_set.has_value(); }

  virtual ~PipelineStageBase() { m_layout.get().m_stages.erase(this); }

private:
  friend class PipelineStageLayout;
  vkw::StrongReference<PipelineStageLayout> m_layout;
  std::optional<vkw::DescriptorSet> m_set;
};

template <typename T>
requires std::derived_from<T, PipelineStageLayout>
class PipelineStage : private PipelineStageBase {
public:
  PipelineStage(T &layout) : PipelineStageBase(layout){};
  T const &layout() const { return dynamic_cast<T const &>(m_get_layout()); };

  vkw::DescriptorSet const &set() const { return m_get_set(); };

  bool hasSet() const { return m_has_set(); }

protected:
  vkw::DescriptorSet &set() { return m_get_set(); };
};
} // namespace RenderEngine
#endif // TESTAPP_PIPELINESTAGE_H
