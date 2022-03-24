#include "Lighting.h"
#include "RenderEngine/RecordingState.h"

void RenderEngine::Lighting::bind(RenderEngine::GraphicsRecordingState &state) const {
    state.setLighting(*this);
}
