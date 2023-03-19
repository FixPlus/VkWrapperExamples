#include "CommonApp.h"
#include "ErrorCallbackWrapper.h"
#include "Planet.hpp"
#include "PlanetSceneWindow.hpp"

using namespace TestApp;

class PlanetApp : public CommonApp {
public:
  PlanetApp()
      : CommonApp(AppCreateInfo{
            .enableValidation = false,
            .applicationName = "Planet",
            .amendDeviceCreateInfo =
                [](vkw::PhysicalDevice &device) {
                  if (device.isFeatureSupported(
                          vkw::PhysicalDevice::feature::fillModeNonSolid))
                    device.enableFeature(
                        vkw::PhysicalDevice::feature::fillModeNonSolid);
                },
            .customWindow = new PlanetSceneWindow(800, 600, "Planet")}),
        m_cam_settings(window<PlanetSceneWindow>().createCameraSettings(gui())),
        m_sunlight(device()), m_sunlightOptions(gui(), m_sunlight),
        m_planetPool(device(), shaderLoader(), m_sunlight,
                     window<PlanetSceneWindow>().camera(), onScreenPass(), 0),
        m_sphereMeshProperties(gui(), m_planetPool.mesh()),
        m_image(textureLoader().loadTexture("earth_color")),
        m_planetTexture(device(), m_planetPool, m_image),
        m_planet(m_planetPool, m_planetTexture),
        m_planetProperties(gui(), m_planet) {
    window<PlanetSceneWindow>().setCameraTarget(m_planet);
    window<PlanetSceneWindow>().createCameraSettings(gui());
  }

protected:
  void onPollEvents() override {
    m_planetPool.update();
    window<PlanetSceneWindow>().update();
  }
  void onMainPass(vkw::PrimaryCommandBuffer &buffer,
                  RenderEngine::GraphicsRecordingState &recorder) override {
    m_planetPool.bindMesh(recorder);
    m_planetPool.setLighting(recorder);
    m_planet.drawSurface(recorder);
    m_planet.drawSkyDome(recorder);
  }

private:
  PlanetCameraSettings m_cam_settings;
  SunLight m_sunlight;
  SunLightProperties m_sunlightOptions;
  PlanetPool m_planetPool;
  SphereMeshOptions m_sphereMeshProperties;
  vkw::Image<vkw::COLOR, vkw::I2D> m_image;
  PlanetTexture m_planetTexture;
  Planet m_planet;
  PlanetProperties m_planetProperties;
};

int runPlanet() {
  PlanetApp app;
  app.run();
  return 0;
}

int main() {
  return ErrorCallbackWrapper<decltype(&runPlanet)>::run(&runPlanet);
}