#ifndef TESTAPP_PIPELINEPOOL_H
#define TESTAPP_PIPELINEPOOL_H

#include "RenderEngine/Pipelines/Geometry.h"
#include "RenderEngine/Pipelines/Projection.h"
#include "RenderEngine/Pipelines/Material.h"
#include "RenderEngine/Pipelines/Lighting.h"
#include "RenderEngine/Pipelines/ShaderLoaderInterface.h"
#include <vkw/PipelineCache.hpp>
#include <map>

namespace RenderEngine{

    class GraphicsPipelinePool{
    public:

        explicit GraphicsPipelinePool(vkw::Device& device, ShaderLoaderInterface& loader): m_device(device), m_cache(device),
                                                                                                 m_shaderLoader(loader) {};

        vkw::PipelineLayout const& layoutOf(GeometryLayout const&geometryLayout,
                                            ProjectionLayout const&projectionLayout, MaterialLayout const&materialLayout, LightingLayout const&lightingLayout);

        vkw::GraphicsPipeline const& pipelineOf(GeometryLayout const&geometryLayout,
                                                ProjectionLayout const&projectionLayout, MaterialLayout const&materialLayout, LightingLayout const&lightingLayout);

    private:

        using M_PipelineKey = std::tuple<GeometryLayout const*, ProjectionLayout const*, MaterialLayout const*, LightingLayout const*>;
        std::reference_wrapper<vkw::Device> m_device;
        std::reference_wrapper<ShaderLoaderInterface> m_shaderLoader;
        vkw::PipelineCache m_cache;
        std::map<M_PipelineKey, vkw::PipelineLayout> m_layouts;
        std::map<M_PipelineKey, vkw::GraphicsPipeline> m_pipelines;
    };

}
#endif //TESTAPP_PIPELINEPOOL_H
