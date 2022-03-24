#include "Projection.h"
#include "RenderEngine/RecordingState.h"

void RenderEngine::Projection::bind(RenderEngine::GraphicsRecordingState &state) const {
    state.setProjection(*this);
}
