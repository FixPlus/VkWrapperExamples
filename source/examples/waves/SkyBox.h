#ifndef TESTAPP_SKYBOX_H
#define TESTAPP_SKYBOX_H


#include "GlobalLayout.h"
#include "common/AssetImport.h"
#include "vkw/Pipeline.hpp"

class SkyBox {
public:

    SkyBox(vkw::Device &device, vkw::RenderPass const &pass, uint32_t subpass, GlobalLayout const &globalLayout,
           TestApp::ShaderLoader const &loader);


    void draw(vkw::CommandBuffer &buffer);

private:

    vkw::GraphicsPipeline m_compile_pipeline(vkw::RenderPass const &pass, uint32_t subpass);

    std::reference_wrapper<vkw::Device> m_device;
    std::reference_wrapper<GlobalLayout const> m_global_layout;
    vkw::VertexShader m_vertexShader;
    vkw::FragmentShader m_fragmentShader;
    vkw::PipelineLayout m_pipelineLayout;
    vkw::GraphicsPipeline m_pipeline;


};

#endif //TESTAPP_SKYBOX_H
