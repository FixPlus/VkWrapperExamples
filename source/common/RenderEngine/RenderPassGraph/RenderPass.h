#ifndef TESTAPP_RENDERPASS_H
#define TESTAPP_RENDERPASS_H

#include "vkw/RenderPass.hpp"

namespace RenderEngine{

    /** Any object allocated and/or used by gpu. */
    class Resource{

    };

    struct AccessInfo{
        VkAccessFlags accessMask;
        VkPipelineStageFlags stageMask;
    };

    /** Pass is arbitrary amount of work put on gpu with it's input and output. */
    class Pass{
    public:
        using ResourceReferenceContainer = std::vector<std::pair<AccessInfo, std::reference_wrapper<Resource>>>;

        ResourceReferenceContainer::const_iterator inputBegin() const{
            return m_inputs.begin();
        }
        ResourceReferenceContainer::const_iterator inputEnd() const{
            return m_inputs.end();
        }
        ResourceReferenceContainer::const_iterator outputBegin() const{
            return m_outputs.begin();
        }
        ResourceReferenceContainer::const_iterator outputEnd() const{
            return m_outputs.end();
        }

    protected:
        ResourceReferenceContainer m_inputs;
        ResourceReferenceContainer m_outputs;
    };
}
#endif //TESTAPP_RENDERPASS_H
