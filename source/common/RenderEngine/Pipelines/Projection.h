#ifndef TESTAPP_PROJECTION_H
#define TESTAPP_PROJECTION_H

#include "RenderEngine/Pipelines/PipelineStage.h"

namespace RenderEngine{

    class ProjectionLayout: public PipelineStageLayout{ ;

    public:
        ProjectionLayout(vkw::Device& device, SubstageDescription const& desc, uint32_t maxSets):
                PipelineStageLayout(device, desc, maxSets){}
    };

    class GraphicsRecordingState;

    class Projection: public PipelineStage<ProjectionLayout>{
    public:
        explicit Projection(ProjectionLayout& layout): PipelineStage<ProjectionLayout>(layout){}

        virtual void bind(GraphicsRecordingState& state) const;

    };
}
#endif //TESTAPP_PROJECTION_H
