#ifndef TESTAPP_PROJECTION_H
#define TESTAPP_PROJECTION_H

#include "RenderEngine/Pipelines/PipelineStage.h"

namespace RenderEngine {

class ProjectionLayout : public PipelineStageLayout {
  ;

public:
  ProjectionLayout(vkw::Device &device, ShaderLoaderInterface &loader,
                   SubstageDescription const &desc, uint32_t maxSets)
      : PipelineStageLayout(device, loader, desc, 1, ".pr.vert", maxSets) {}
};

class GraphicsRecordingState;

class Projection : public PipelineStage<ProjectionLayout> {
public:
  explicit Projection(ProjectionLayout &layout)
      : PipelineStage<ProjectionLayout>(layout) {}

  virtual void bind(GraphicsRecordingState &state) const;
};
} // namespace RenderEngine
#endif // TESTAPP_PROJECTION_H
