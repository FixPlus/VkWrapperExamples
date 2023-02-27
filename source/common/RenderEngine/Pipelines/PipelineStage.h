#ifndef TESTAPP_PIPELINESTAGE_H
#define TESTAPP_PIPELINESTAGE_H

#include <string>
#include <vkw/DescriptorSet.hpp>
#include <vkw/DescriptorPool.hpp>
#include <set>
#include <optional>

namespace RenderEngine{

    struct SubstageDescription{
        std::string shaderSubstageName;
        boost::container::small_vector<vkw::DescriptorSetLayoutBinding, 3> setBindings;
        boost::container::small_vector<VkPushConstantRange, 3> pushConstants;
    };

    class PipelineStageBase;

    class PipelineStageLayout: public vkw::ReferenceGuard{
    public:
        PipelineStageLayout(vkw::Device& device, SubstageDescription desc, uint32_t maxSets): m_layout(device,
                                                                                                       desc.setBindings, 0),
                                                                                              m_description(std::move(desc)){
            if(!m_description.setBindings.empty())
                m_pool.emplace(m_initPool(device, m_description, maxSets));
        }

        PipelineStageLayout(PipelineStageLayout&& another) noexcept;
        PipelineStageLayout& operator=(PipelineStageLayout&& another) noexcept;

        vkw::DescriptorSetLayout const& layout() const{
            return m_layout;
        }

        SubstageDescription const& description() const{
            return m_description;
        }

        virtual ~PipelineStageLayout() = default;
    private:
        static vkw::DescriptorPool m_initPool(vkw::Device& device, SubstageDescription const & createInfo, uint32_t maxSets);

        friend class PipelineStageBase;

        std::set<PipelineStageBase*> m_stages;
        vkw::DescriptorSetLayout m_layout;
        std::optional<vkw::DescriptorPool> m_pool;
        SubstageDescription m_description;
    };

    class PipelineStageBase{
    public:
        PipelineStageBase(PipelineStageLayout& layout): m_layout(layout){
            if(layout.m_pool)
                m_set.emplace(layout.m_pool.value(), layout.m_layout);
            m_layout.get().m_stages.emplace(this);
        }

        PipelineStageBase(PipelineStageBase&& another) noexcept: m_layout(another.m_layout), m_set(std::move(another.m_set)){
            m_layout.get().m_stages.emplace(this);
        }

        PipelineStageBase& operator=(PipelineStageBase&& another) noexcept{
            m_layout = another.m_layout;
            m_set = std::move(another.m_set);
            m_layout.get().m_stages.emplace(this);
            return *this;
        }

        PipelineStageLayout const& m_get_layout() const{
            return m_layout;
        };

        vkw::DescriptorSet const& m_get_set() const{
            return m_set.value();
        };

        vkw::DescriptorSet& m_get_set() {
            return m_set.value();
        };

        bool m_has_set() const{
            return m_set.has_value();
        }

        virtual ~PipelineStageBase(){
            m_layout.get().m_stages.erase(this);
        }
    private:
        friend class PipelineStageLayout;
        vkw::StrongReference<PipelineStageLayout> m_layout;
        std::optional<vkw::DescriptorSet> m_set;
    };

    template<typename T>
    requires std::derived_from<T, PipelineStageLayout>
    class PipelineStage: private PipelineStageBase{
    public:
        PipelineStage(T& layout): PipelineStageBase(layout){};
        T const& layout() const{
            return dynamic_cast<T const&>(m_get_layout());
        };

        vkw::DescriptorSet const& set() const{
            return m_get_set();
        };

        bool hasSet() const{
            return m_has_set();
        }
    protected:
        vkw::DescriptorSet& set() {
            return m_get_set();
        };
    };
}
#endif //TESTAPP_PIPELINESTAGE_H
