#include "CommonApp.h"
#include "ErrorCallbackWrapper.h"
#include "Fractal.h"

using namespace TestApp;

class FractalApp final : public CommonApp {
public:
  FractalApp()
      : CommonApp{AppCreateInfo{true, "Fractal"}},
        m_fractal(device(), onScreenPass(), 0, shaderLoader(), textureLoader(),
                  currentSurfaceExtents().width,
                  currentSurfaceExtents().height),
        m_fractalSettings(gui(), m_fractal) {}

protected:
  void preMainPass(vkw::PrimaryCommandBuffer &buffer,
                   RenderEngine::GraphicsPipelinePool &pool) override {
    m_fractal.drawOffscreen(buffer, pool);
  }

  void onMainPass(vkw::PrimaryCommandBuffer &buffer,
                  RenderEngine::GraphicsRecordingState &recorder) override {
    m_fractal.draw(recorder);
  }

  void onFramebufferResize() override {
    m_fractal.resizeOffscreenBuffer(currentSurfaceExtents().width,
                                    currentSurfaceExtents().height);
  };
  void onPollEvents() override { m_fractal.update(window().camera()); }

private:
  Fractal m_fractal;
  FractalSettings m_fractalSettings;
};

int runFractal() {
  FractalApp app{};
  app.run();
  return 0;
}

int main() {
  return ErrorCallbackWrapper<decltype(&runFractal)>::run(&runFractal);
}