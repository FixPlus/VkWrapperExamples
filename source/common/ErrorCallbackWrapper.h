#ifndef TESTAPP_ERRORCALLBACKWRAPPER_H
#define TESTAPP_ERRORCALLBACKWRAPPER_H

#include "RenderEngine/Window/Boxer.h"
#include <functional>
#include <vkw/Exception.hpp>

using PFN_int_void = int (*)();
template<PFN_int_void function>
class ErrorCallbackWrapper{
public:
    static int run() {
        try{
            return function();
        }
        catch(vkw::VulkanError& e){
            RenderEngine::Boxer::show(e.what(), "Vulkan API error", RenderEngine::Boxer::Style::Error);
        }
        catch(vkw::ExtensionUnsupported& e){
            RenderEngine::Boxer::show(e.what(), "Unsupported extension", RenderEngine::Boxer::Style::Error);
        }
        catch(vkw::FeatureUnsupported& e){
            RenderEngine::Boxer::show(e.what(), "Unsupported feature", RenderEngine::Boxer::Style::Error);
        }
        catch(vkw::ExtensionMissing& e){
            RenderEngine::Boxer::show(e.what(), "Missing extension", RenderEngine::Boxer::Style::Error);
        }
        catch(vkw::Error& e){
            RenderEngine::Boxer::show(e.what(), "vkw::Error", RenderEngine::Boxer::Style::Error);
        }
        catch(std::runtime_error& e){
            RenderEngine::Boxer::show(e.what(), "Fatal error", RenderEngine::Boxer::Style::Error);
        }
        return -1;
    }
};
#endif //TESTAPP_ERRORCALLBACKWRAPPER_H
