#include "RecordingState.h"


namespace RenderEngine{
    void GraphicsRecordingState::setGeometry(Geometry const& geometry){
        if(m_geometry == &geometry)
            return;

        m_geometry = &geometry;

        if(&m_geometry->layout() != m_geometry_layout){
            m_geometry_layout = &m_geometry->layout();
            need_set_pipeline = true;
        }

       if(m_geometry->hasSet()){
            need_update_geometry_set = true;
        }
    }

    void GraphicsRecordingState::setProjection(Projection const& projection){
        if(m_projection == &projection)
            return;

        m_projection = &projection;

        if(&m_projection->layout() != m_projection_layout){
            m_projection_layout = &m_projection->layout();
            need_set_pipeline = true;
        }

        if(m_projection->hasSet()){
            need_update_projection_set = true;
        }

    }

    void GraphicsRecordingState::setMaterial(Material const& material){
        if(m_material == &material)
            return;

        m_material = &material;

        if(&m_material->layout() != m_material_layout){
            m_material_layout = &m_material->layout();
            need_set_pipeline = true;
        }

         if(m_material->hasSet()){
            need_update_material_set = true;
         }
    }

    void GraphicsRecordingState::setLighting(Lighting const& lighting){
        if(m_lighting == &lighting)
            return;

        m_lighting = &lighting;

        if(&m_lighting->layout() != m_lighting_layout){
            m_lighting_layout = &m_lighting->layout();
            need_set_pipeline = true;
        }

        if(m_lighting->hasSet()) {
            need_update_lighting_set = true;
        }
    }

    void GraphicsRecordingState::bindPipeline() {
        if(!m_pipeline_ready())
            throw std::runtime_error("Cannot bind pipeline: not all stages are specified yet");

        if(need_set_pipeline) {
            m_commandBuffer.get().bindGraphicsPipeline(
                    m_pool.get().pipelineOf(*m_geometry_layout, *m_projection_layout, *m_material_layout,
                                            *m_lighting_layout));
            need_set_pipeline = false;
        }
        m_set_all_sets();
    }

}