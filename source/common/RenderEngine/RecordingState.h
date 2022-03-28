#ifndef TESTAPP_RECORDINGSTATE_H
#define TESTAPP_RECORDINGSTATE_H

#include <RenderEngine/Pipelines/PipelinePool.h>


namespace RenderEngine{


    class GraphicsRecordingState{
    public:
        GraphicsRecordingState(vkw::CommandBuffer& command, GraphicsPipelinePool& pool): m_commandBuffer(command),
                                                                                         m_pool(pool){

        }

        void reset() {
            m_geometry = nullptr;
            need_update_geometry_set = true;
            m_projection = nullptr;
            need_update_projection_set = true;
            m_material = nullptr;
            need_update_material_set = true;
            m_lighting = nullptr;
            need_update_lighting_set = true;
            need_set_pipeline = true;
            m_geometry_layout = nullptr;
            m_projection_layout = nullptr;
            m_material_layout = nullptr;
            m_lighting_layout = nullptr;
        }

        void setGeometry(Geometry const& geometry);

        void setProjection(Projection const& projection);

        void setMaterial(Material const& material);

        void setLighting(Lighting const& lighting);

        void bindPipeline();

        vkw::CommandBuffer& commands() { return m_commandBuffer.get();}

        template<typename T>
        void pushConstants(T constant, VkShaderStageFlagBits shaderStage, uint32_t offset){
            m_commandBuffer.get().template pushConstants(m_current_layout(), shaderStage, offset, constant);
        }

    private:
        void m_set_all_sets();
        bool m_pipeline_ready() const {
            return m_geometry && m_projection && m_material && m_lighting;
        }

        vkw::PipelineLayout const& m_current_layout();

        Geometry const* m_geometry = nullptr;
        bool need_update_geometry_set = true;
        Projection const* m_projection = nullptr;
        bool need_update_projection_set = true;
        Material const* m_material = nullptr;
        bool need_update_material_set = true;
        Lighting const* m_lighting = nullptr;
        bool need_update_lighting_set = true;
        bool need_set_pipeline = true;
        GeometryLayout const* m_geometry_layout = nullptr;
        ProjectionLayout const* m_projection_layout = nullptr;
        MaterialLayout const* m_material_layout = nullptr;
        LightingLayout const* m_lighting_layout = nullptr;
        std::reference_wrapper<vkw::CommandBuffer> m_commandBuffer;
        std::reference_wrapper<GraphicsPipelinePool> m_pool;
    };
}
#endif //TESTAPP_RECORDINGSTATE_H
